//Vasco Alves 2022228207 Joao Neto 2023234004
#include "deichain.h"
#include <stdlib.h>

int running = 1;

void val_cleanup(int retv) {
  c_cleanup_globals();
  puts((retv == 0) ? "Validator: Exited successfully!\n" : "Validator: FAILED!\n");
  exit(retv);
}

void val_handle_signit(int sig) {
  if (sig != SIGINT) return;
  shutdown = 1; // Signal threads to exit
  sigint_received = 1; // Signal main thread to exit
}

void c_val_main() {

  assert(global.mq_statistics != (mqd_t)-1);

  struct sigaction sa;
  sa.sa_handler = val_handle_signit;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);
   
  /* Open pipe (not blocking to check errors) */
  int pipe_validator_fd = open(PIPE_VALIDATOR, O_RDONLY | O_NONBLOCK);
  if (pipe_validator_fd < 0) {
    puts("Validator: Failed to open FIFO\n");
    val_cleanup(1);
  }
  close(pipe_validator_fd);

  /* Open pipe for real this time */
  pipe_validator_fd = open(PIPE_VALIDATOR, O_RDONLY );
  unsigned char data_recieved[SIZE_BLOCK];
  TransactionBlock block_recieved = (void*) data_recieved;
  BlockInfo *block_info = (BlockInfo*) block_recieved;

  hashstring_t hashstring[HASH_STRING_SIZE];

  while (shutdown == 0) {
    /* Reading blocks the pipe */
    unsigned long int count = read(pipe_validator_fd, block_recieved, SIZE_BLOCK);
    if (shutdown != 0) {
      break;
    }

    if (count < SIZE_BLOCK) {
      c_logputs("[Validator] READ ERROR\n");
      continue;
    }

    c_pow_hash_to_string(block_info->txb_id, hashstring);
    int valid = c_pow_verify_nonce(block_recieved);
    c_logprintf("[Validator] Received block id=%.20s :: block is %s\n", hashstring, (valid == 1) ? "valid" : "NOT valid" );
    int miner_id_received;
    memcpy(&miner_id_received,block_info->txb_id, sizeof(int));
    printf("This blocks comes from miner thread: %d\n", miner_id_received);
    int invalid_block =0;
    Transaction* first =(Transaction*)( block_recieved + sizeof(BlockInfo));
    Transaction* last =first + config.transactions_per_block;
    TransactionPool* pool_first = global.shmem_pool_data;
    TransactionPool* pool_last = (global.shmem_pool_data+ config.pool_size);
    for (; first < last; first++) {
      hash_t* id = first->tx_id;
      if(invalid_block==1)
        break;
      for (pool_first = global.shmem_pool_data; pool_first < pool_last; pool_first++) {
        int match = 1;
        for (size_t i = 0; i < HASH_SIZE; i++) {
          if (id[i] != pool_first->tx.tx_id[i]) {
            match = 0;
            break;
          }
        }
        while (match == 1) {
          
          if(sem_trywait(global.sem_pool_full)==0){
          pool_first->empty = 0;
          sem_post(global.sem_pool_empty);
          break;
          }
          
        }
        if(match ==1){
          c_logprintf("Found Transaction\n");
          break;
        }
        if(pool_first == pool_last-1){
          c_logprintf("NOT FOUND\n");
          invalid_block = 1;
          break;
        }
      }
    }

  }

  val_cleanup(EXIT_SUCCESS);
}
