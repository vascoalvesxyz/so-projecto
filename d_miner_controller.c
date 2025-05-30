//Vasco Alves 2022228207 Joao Neto 2023234004
#include "deichain.h"
#include <stdio.h>

typedef struct MinerThreadArgs {
  int id;
  int pipe_validator_fd;
} MinerThreadArgs;

typedef struct {
  pthread_t *mc_threads;      // 8 bytes, pointer to thread array
  MinerThreadArgs *args_array;// 8 bytes, pointer to id array
  uint32_t created_threads;   // 4 bytes, created threads
  uint32_t miners_max;        // 4 bytes, max num of miners
} mc_vars_t;

void* miner_thread(void* recv) {

  MinerThreadArgs args = *((MinerThreadArgs*) recv);

  uint32_t uid = 0;
  unsigned int transaction_n = 0;
  TransactionPool *pool_ptr = global.shmem_pool_data; 
  TransactionBlock transaction_block = calloc(1, SIZE_BLOCK); 
  BlockInfo   *new_block = transaction_block;
  Transaction *transaction_array = transaction_block+sizeof(BlockInfo);
  //sem_post(global.sem_pool_full);
  while (shutdown == 0) {

    if (sem_trywait(global.sem_pool_full) == 0) {
      sem_post(global.sem_pool_full);
      #ifdef DEBUG
      printf("[DEBUG] [Miner Thread %d] Waiting...\n", args.id);
      #endif
      if (shutdown == 1)
        break;
      // sem_wait(global.sem_pool_mutex);
      if (shutdown == 1) break;

      /* Find Transaction */
      
      for (uint32_t i = config.pool_size-1; i > 1 ; i--) {
        int same=0;
        if (pool_ptr[i].empty == 1) {
          for(unsigned int j =0; j<transaction_n;j++){
            if(c_pow_hash_equals(pool_ptr[i].tx.tx_id,transaction_array[j].tx_id)==0)
            same =1;
          }
          if(same == 1){
            continue;
          }
          // pool_ptr[i].empty = 0;
          #ifdef DEBUG
          printf("[DEBUG] [Miner Thread %d] Grabbing transaction from slot: %d\n", args.id, i);
          #endif
          transaction_array[transaction_n] = pool_ptr[i].tx;
          transaction_n++;
          break;
        }
      }
      //sem_post(global.sem_pool_mutex);
      //sem_post(global.sem_pool_empty);

      /* If transaction_array is full, send new block to validator */
      if (transaction_n == config.transactions_per_block) {

        /* Create Unique ID */ 
        uint32_t input = args.id+uid;
        uid++;

        /* Hash id */
        SHA256( (void*) &input, sizeof(uint32_t), &new_block->txb_id[0]);
        new_block->timestamp = time(NULL);
        new_block->nonce = 0;

        memcpy(new_block->txb_id, &(args.id), sizeof(int));

        for (uint32_t i = 0; i < config.blockchain_blocks; i++) {
          
          /* Found block */
          if (global.shmem_blockchain_data[i].nonce == 0) {
            if (i == 0) {
              memcpy(new_block->previous_block_hash, new_block->txb_id, HASH_SIZE);
            } else {
              c_pow_hash_compute(&global.shmem_blockchain_data[i], new_block->previous_block_hash);
            }
            break;
          }
          
        }  

        /* Perform Proof of Work */
        PoWResult result = c_pow_proofofwork(transaction_block);
        c_logprintf("[Miner Thread %d] Proof of work completed in %lfms after %ld operations.\n", args.id, result.elapsed_time, result.operations );

        /* Serialize memory in heap and write */
        #define SIZE_BLOCK_PLUS_POW SIZE_BLOCK + sizeof(PoWResult)

        unsigned char data_send[SIZE_BLOCK_PLUS_POW];
        memset(data_send, 0, SIZE_BLOCK_PLUS_POW);
        memcpy(data_send, transaction_block, SIZE_BLOCK);
        memcpy(data_send+SIZE_BLOCK, &result, sizeof(PoWResult));

        if(write(args.pipe_validator_fd, data_send, SIZE_BLOCK_PLUS_POW) > 0) {
          c_logprintf("[Miner Thread %d] Wrote a block to validator.\n", args.id);
        }

        transaction_n = 0;
      }

    } else if (errno == EAGAIN) {
      sleep(1);
    } else {
      perror("sem_trywait failed");
      break;
    }

  }

  c_logprintf("[Miner Thread %d] Exiting thread.\n", args.id);
  free(transaction_block);
  pthread_exit(NULL);
}

void mc_handle_sigint(int sig) {
  if (sig != SIGINT) return;
  shutdown = 1; // Signal threads to exit
  sigint_received = 1; // Signal main thread to exit
#ifdef DEBUG
  puts("[DEBUG] [Miner controller] SIGINT recieved");
#endif
}

/* Function Declarations */
void c_mc_main(unsigned int miners_max) {

  mc_vars_t vars = (mc_vars_t) {
    .mc_threads = malloc(sizeof(pthread_t) * miners_max ),
    .args_array   = malloc(sizeof(MinerThreadArgs) * miners_max ),
    .created_threads = 0,
    .miners_max = miners_max
  };

  int pipe_validator_fd = open(PIPE_VALIDATOR, O_WRONLY);

  /* Create Miner Threads */
  for (uint32_t i = 0; i < miners_max; i++) {
    vars.args_array[i] = (MinerThreadArgs) {i, pipe_validator_fd};
    if (pthread_create(&vars.mc_threads[i], NULL, miner_thread, (void*) &vars.args_array[i]) != 0) {
      perror("pthread_create failed");
      break; // Stop creating threads on failure
    }
    vars.created_threads++;
  }

  struct sigaction sa;
  sa.sa_handler = mc_handle_sigint;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);

  c_logputs("[Miner controller] Started successfully.\n");

  /* Wait for shutdown signal */
  while (!sigint_received) {
    pause();
  }

  shutdown = 1; // Ensure threads exit

  /* Send shutdown signal */
  for (uint32_t i = 0; i < vars.created_threads; i++) {
    sem_post(global.sem_pool_full); // Unblock sem_wait
    sem_post(global.sem_pool_empty);
  }

  /* Clean up threads */
  for (uint32_t i = 0; i < vars.created_threads; i++) {
    if (vars.mc_threads[i]) {
      pthread_join(vars.mc_threads[i], NULL);
    }
  }

  if (vars.mc_threads) free(vars.mc_threads);
  if(vars.args_array) free(vars.args_array);

  c_cleanup_globals(); 
  c_logputs("[Miner controller] Exit successful.\n");
  exit(EXIT_SUCCESS);
}
