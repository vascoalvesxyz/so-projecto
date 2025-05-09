//Vasco Alves 2022228207 Joao Neto 2023234004
#include "deichain.h"

typedef struct MinerThreadArgs {
  int id;
} MinerThreadArgs;

unsigned int i = 0, created_threads = 0;
pthread_t *mc_threads;
int *id_array;
unsigned int uid = 0;

void* miner_thread(void* id_ptr) {
  int id = *( (int*) id_ptr );

  unsigned int transaction_n = 0;
  int pool_size = g_pool_size;
  TransactionPool *pool_ptr = g_shmem_pool_data; 

  TransactionBlock transaction_block = calloc(1, SIZE_BLOCK); 
  BlockInfo   *new_block = transaction_block;
  Transaction *transaction_array = transaction_block+sizeof(BlockInfo);

  while (shutdown == 0) {

    if (sem_trywait(g_sem_pool_full) == 0) {
      // Successfully decremented semaphore
      printf("[Miner Thread %d] Waiting...\n", id);
      if (shutdown == 1)
        break;

      // sem_wait(g_sem_pool_mutex);
      if (shutdown == 1) break;

      /* Find Transaction */
      for (unsigned i = pool_size-1; i > 1 ; i--) {
        if (pool_ptr[i].empty == 1) {
          // pool_ptr[i].empty = 0;
          printf("[Miner Thread %d] Grabbing transaction from slot: %d\n", id, i);
          transaction_array[transaction_n] = pool_ptr[i].tx;
          transaction_n++;
          break;
        }
      }
      // sem_post(g_sem_pool_mutex);

      /* If transaction_array is full, send new block to validator */
      if (transaction_n == g_transactions_per_block) {

        uint32_t input = id+uid;
        uid++;

        SHA256( (void*) &input, sizeof(uint32_t), &new_block->txb_id[0]);
        new_block->timestamp = time(NULL);
        new_block->nonce = 0;


        puts("[FYI] The proof is not working");

        /* Serialize memory in heap and write */
        unsigned char data_send[SIZE_BLOCK];
        memset(data_send, 0, SIZE_BLOCK);
        memcpy(data_send, transaction_block, SIZE_BLOCK);
        write(pipe_validator_fd, data_send, SIZE_BLOCK);

        printf("[Miner Thread %d] WROTE A BLOCK TO VALIDATOR\n", id);
        transaction_n = 0;
      }


      sem_post(g_sem_pool_empty);
    } else if (errno == EAGAIN) {
      sleep(1);
    } else {
      perror("sem_trywait failed");
      break;
    }

  }

  printf("[Miner Thread %d] Shutdown set to 0, exiting thread.\n", id);
  free(transaction_block);
  // mc_threads[id-1] = NULL; // ?
  pthread_exit(NULL);
}

void mc_cleanup() {
  shutdown = 1; // Ensure threads exit

#ifdef DEBUG
  puts("[DEBUG] [Miner controller] Sending shutdown signal");
#endif

  for (i = 0; i < created_threads; i++) {
    sem_post(g_sem_pool_full); // Unblock sem_wait
    sem_post(g_sem_pool_empty);
  }

#ifdef DEBUG
  puts("[DEBUG] [Miner controller] Joining threads.");
#endif

  // Clean up threads
  for (unsigned i = 0; i < created_threads; i++) {
    if (mc_threads[i]) {
      pthread_join(mc_threads[i], NULL);
    }
  }

#ifdef DEBUG
  puts("[DEBUG] [Miner controller] Freeing resources");
#endif

  if (pipe_validator_fd >= 0)
    close(pipe_validator_fd);

  free(mc_threads);
  free(id_array);
  c_cleanup(); // Cleanup semaphores/shared memory
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

  mc_threads  = malloc(sizeof(pthread_t) * miners_max );
  id_array    = malloc(sizeof(int) * miners_max );

  pipe_validator_fd = open(PIPE_VALIDATOR, O_WRONLY);

  /* Create Miner Threads */
  for (i = 0; i < miners_max; i++) {
    id_array[i] = i+1;
    if (pthread_create(&mc_threads[i], NULL, miner_thread, &id_array[i]) != 0) {
      perror("pthread_create failed");
      break; // Stop creating threads on failure
    }
    created_threads++;
  }

  // Set up signal handler
  struct sigaction sa;
  sa.sa_handler = mc_handle_sigint;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);

#ifdef DEBUG
  puts("[DEBUG] [Miner controller] Entering main loop");
#endif

  // Wait for shutdown signal
  while (!sigint_received) {
    pause();
  }

#ifdef DEBUG
  puts("[DEBUG] [Miner controller] Starting cleanup");
#endif

  mc_cleanup();

#ifdef DEBUG
  puts("[DEBUG] [Miner controller] Exit complete");
#endif
  exit(0);
}
