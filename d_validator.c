//Vasco Alves 2022228207 Joao Neto 2023234004
#include "deichain.h"
#include <semaphore.h>
#include <stdint.h>
#include <stdlib.h>

#define MAX_VAL_THREADS 3
#define SIZE_BLOCK_PLUS_POW SIZE_BLOCK + sizeof(PoWResult)

typedef struct {
  pthread_t threads[MAX_VAL_THREADS];
  uint32_t created_threads;
  uint32_t desired_threads;
  int pipe_fd;
} val_vars_t;

typedef struct {
  val_vars_t *vars;   // pointer to validator stack
  uint32_t thread_id; // Unique identifier for this validator thread
} ValidatorThreadArgs ;

void val_cleanup(int retv);
void val_handle_signit(int sig);
void* validator_thread_func(void* arg);
void* val_thread_controller_thread(void* arg);

void val_cleanup(int retv) {
  c_cleanup_globals();
  c_logputs((retv == 0) ? "[Validator] Exited successfully!\n" : "[Validator] FAILED!\n");
  exit(retv);
}

void val_handle_signit(int sig) {
  if (sig != SIGINT) return;
  shutdown = 1; // Signal threads to exit
  sigint_received = 1; // Signal main thread to exit
}

void* val_thread_controller_thread(void* arg) {

  /* Parse arguments */
  ValidatorThreadArgs thread_args = *(ValidatorThreadArgs*) arg;
  val_vars_t *vars = thread_args.vars;

  ValidatorThreadArgs args_array[MAX_VAL_THREADS];

  int sem_value;
  sem_getvalue(global.sem_pool_full, &sem_value);

  c_logputs("[Validator Controller] Validator Controller Started\n");

  while (vars->desired_threads > 0 && shutdown == 0) {
    /* Create new threads if needed */
    if (vars->created_threads < vars->desired_threads) {
      for (uint32_t new_id = vars->created_threads; vars->created_threads < vars->desired_threads; new_id++ ) {
        args_array[new_id] = (ValidatorThreadArgs) {vars, new_id};
        pthread_create(&vars->threads[new_id], NULL, validator_thread_func, (void*) &args_array[new_id]);
        c_logprintf("[Validator] Created validator thread %d\n", new_id);
        vars->created_threads++;
      }
    }
    
    /* Wait for pool to have transactions */
    if(sem_trywait(global.sem_pool_full)!=0){ // decrement 
      sleep(1);
      continue;
    }

    if (vars->desired_threads == 0) break;
    sem_post(global.sem_pool_full); // increment immediately

    /* Adjust number of threads based on pool size */
    sem_getvalue(global.sem_pool_full, &sem_value);
    /* 80% - 100% */
    if (sem_value >= (uint32_t) config.pool_size * 0.80) {
      vars->desired_threads = 3;
    } 
    /* 60%+ */
    else if (sem_value >= (uint32_t) config.pool_size * 0.60) {
      vars->desired_threads = 2;
    }
    /* 40% */
    else if (sem_value <= (uint32_t) config.pool_size * 0.40) {
      vars->desired_threads = 1;
    }

  }

  for (uint32_t i = 0; i < vars->created_threads; i++) {
    pthread_join(vars->threads[i], NULL);
  }

  c_logputs("[Validator Controller] All Validators Joined\n");
  c_logputs("[Validator Controller] Exiting\n");
  pthread_exit(NULL);
}

void* validator_thread_func(void* arg) {

  /* Parse arguments */
  ValidatorThreadArgs thread_args = *(ValidatorThreadArgs*) arg;
  val_vars_t *vars = thread_args.vars;
  int pipe_fd = vars->pipe_fd;
  uint32_t thread_id = thread_args.thread_id;

  unsigned char data_received[SIZE_BLOCK_PLUS_POW];
  TransactionBlock block_received = (void*)data_received;
  BlockInfo* block_info = (BlockInfo*)block_received;

  hashstring_t hashstring[HASH_STRING_SIZE];

  c_logprintf("[Validator Thread  %u] Thread started\n", thread_id);

  sem_wait(global.sem_pool_full);
#ifdef DEBUG
  printf("[DEBUG] [Validator Thread %u] thread Waiting", thread_id);
#endif /* ifdef DEBUG */
  sem_post(global.sem_pool_full);

  while (thread_id < vars->desired_threads && shutdown == 0) {

    ssize_t count = read(pipe_fd, block_received, SIZE_BLOCK_PLUS_POW);
    if (shutdown != 0) break; // break in case pipe closes

    if (count == -1) {
      c_logprintf("[Validator Thread %u] Read error: %s\n", thread_id, strerror(errno));
      continue;
    }

    // Verify block
    c_pow_hash_to_string(block_info->txb_id, hashstring);
    int pow_valid = c_pow_verify_nonce(block_received);

    c_logprintf("[Validator Thread %u] Block %.20s from miner %d: POW %s\n",
                thread_id, hashstring, 
                *(int*)block_info->txb_id,
                pow_valid ? "VALID" : "INVALID");

    // Validate transactions
    int tx_valid = 1;
    Transaction* tx_start = (Transaction*)(block_received + sizeof(BlockInfo));
    Transaction* tx_end = tx_start + config.transactions_per_block;

    for (Transaction* tx = tx_start; tx < tx_end && tx_valid; tx++) {
      int found = 0;
      for (TransactionPool* pool = global.shmem_pool_data; 
      pool < global.shmem_pool_data + config.pool_size; 
      pool++) {
        if (memcmp(tx->tx_id, pool->tx.tx_id, HASH_SIZE) == 0) {
          if (sem_trywait(global.sem_pool_full) == 0) {
            if(pool->empty ==0){
              sem_post(global.sem_pool_full);
              break;}
            sem_wait(global.sem_pool_mutex);
            pool->empty = 0;
            sem_post(global.sem_pool_mutex);
            printf("%ld\n", pool-global.shmem_pool_data);
            sem_post(global.sem_pool_empty);
            found = 1;
            break;
          }
          sem_post(global.sem_pool_full);
        }
      }
      if (!found) {
        tx_valid = 0;
        c_logprintf("[Validator Thread %u] Invalid TX\n", 
                    thread_id);
      }
    }

    PoWResult powres = *((PoWResult*)(block_received + SIZE_BLOCK));

    // Send statistics
    MinerBlockInfo stats_msg;
    memset(&stats_msg, 0, sizeof(MinerBlockInfo));
    stats_msg = (MinerBlockInfo) {
      .miner_id = *(int*) block_info->txb_id,
      .valid_blocks = (pow_valid && tx_valid) ? 1 : 0,
      .invalid_blocks = (pow_valid && tx_valid) ? 0 : 1,
      .pow_time = powres.elapsed_time,
      .total_blocks = 1
    };

    if(pow_valid && tx_valid){
      unsigned char *point =(unsigned char *)global.shmem_pool_data;
      while(((BlockInfo*)point)->nonce !=0)
        point += SIZE_BLOCK;

      printf("nonce -> %ld \n", ((BlockInfo*)point)->nonce);

      memcpy(global.shmem_blockchain_data, block_info, SIZE_BLOCK);

    }
    if (mq_send(global.mq_statistics, (char*)&stats_msg, sizeof(stats_msg), 0) < 0) {
      c_logprintf("[Validator Thread %u] Failed to send stats\n",
                  thread_id);
    }

    if (!tx_valid) {
      c_logprintf("[Validator Thread %u] Block contains invalid transactions\n", thread_id);
    }
  }

  c_logprintf("[Validator Thread  %u] Thread exiting\n", thread_id);
  vars->created_threads--;
  pthread_exit(NULL);
}

void c_val_main() {

  /* Try to open pipe */
  int pipe_validator_fd = open(PIPE_VALIDATOR, O_RDONLY | O_NONBLOCK);
  if (pipe_validator_fd < 0) {
    c_logputs("Validator: Failed to open FIFO\n");
    val_cleanup(1);
  }
  close(pipe_validator_fd);

  pipe_validator_fd = open(PIPE_VALIDATOR, O_RDONLY);
  if (pipe_validator_fd < 0) {
    c_logputs("Validator: Failed to open FIFO\n");
    val_cleanup(1);
  }

  /* Set vars */
  val_vars_t vars = { {0}, 0, 1, pipe_validator_fd};
  vars.desired_threads = 1;

  c_logputs("[Validator] Starting validator controller...\n");

  /* Create validator controller thread */
  pthread_t  controller_thread;
  ValidatorThreadArgs controller_thread_args = (ValidatorThreadArgs) {&vars, 0};
  if (pthread_create(&controller_thread, NULL, val_thread_controller_thread, (void*) &controller_thread_args) != 0) {
    close(pipe_validator_fd);
    val_cleanup(EXIT_FAILURE);
  }


  /* Handle Sigint */
  struct sigaction sa;
  sa.sa_handler = val_handle_signit;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);


  /* Main loop: Handle Signals */
  while (sigint_received == 0) {
    pause();
    if (shutdown == 1) break;
  }

  vars.desired_threads = 0;
  sem_post(global.sem_pool_full); // Unblock sem_wait
  sem_post(global.sem_pool_empty);
  pthread_join(controller_thread, NULL);

  /* Clean up */
  close(pipe_validator_fd);
  val_cleanup(EXIT_SUCCESS);
}
