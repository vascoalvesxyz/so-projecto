//Vasco Alves 2022228207 Joao Neto 2023234004
#include "deichain.h"
#include <stdlib.h>
typedef struct {
    int pipe_fd;        // Pipe file descriptor for reading blocks
    uint32_t thread_id; // Unique identifier for this validator thread
} ValidatorThreadArgs;


void val_cleanup(int retv) {
  //c_cleanup_globals();
  puts((retv == 0) ? "Validator: Exited successfully!\n" : "Validator: FAILED!\n");
  exit(retv);
}

void val_handle_signit(int sig) {
  if (sig != SIGINT) return;
  shutdown = 1; // Signal threads to exit
  sigint_received = 1; // Signal main thread to exit
}
void* validator_thread_func(void* arg) {
    ValidatorThreadArgs* args = (ValidatorThreadArgs*)arg;
    int pipe_fd = args->pipe_fd;
    uint32_t thread_id = args->thread_id;
    
    unsigned char data_received[SIZE_BLOCK];
    TransactionBlock block_received = (void*)data_received;
    BlockInfo* block_info = (BlockInfo*)block_received;
    hashstring_t hashstring[HASH_STRING_SIZE];

    c_logprintf("[Validator-%u] Thread started\n", thread_id);

    while (!shutdown || thread_id> ((ValidatorThreadArgs*)arg)->thread_id) {
        ssize_t count = read(pipe_fd, block_received, SIZE_BLOCK);
        
        if (shutdown) break;

        if (count == -1) {
            c_logprintf("[Validator-%u] Read error: %s\n", thread_id, strerror(errno));
            continue;
        }


        // Verify block
        c_pow_hash_to_string(block_info->txb_id, hashstring);
        int pow_valid = c_pow_verify_nonce(block_received);
        
        c_logprintf("[Validator-%u] Block %.20s from miner %d: POW %s\n",
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
                        pool->empty = 0;
                        printf("%ld\n", pool-global.shmem_pool_data);
                        sem_post(global.sem_pool_empty);
                        found = 1;
                        break;
                    }
                }
            }
            if (!found) {
                tx_valid = 0;
                c_logprintf("[Validator-%u] Invalid TX\n", 
                           thread_id);
            }
        }

        // Send statistics
        MinerBlockInfo stats_msg = {
            .miner_id = *(int*)block_info->txb_id,
            .valid_blocks = (pow_valid && tx_valid) ? 1 : 0,
            .invalid_blocks = (pow_valid && tx_valid) ? 0 : 1,
            .timestamp = block_info->timestamp,
            .total_blocks = 1
        };

        if (mq_send(global.mq_statistics, (char*)&stats_msg, sizeof(stats_msg), 0) < 0) {
            c_logprintf("[Validator-%u] Failed to send stats\n",
                       thread_id);
        }

        if (!tx_valid) {
            c_logprintf("[Validator-%u] Block contains invalid transactions\n", thread_id);
        }
    }

    c_logprintf("[Validator-%u] Thread exiting\n", thread_id);
     // Clean up the argument struct
    pthread_exit(NULL);
}

void c_val_main() {
    

    // Set up signal handler
    struct sigaction sa;
    sa.sa_handler = val_handle_signit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    // Open pipe
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

    // Open message queue
    


    pthread_t validator_threads[3];
    ValidatorThreadArgs* thread_args= malloc(sizeof(ValidatorThreadArgs));
    thread_args->thread_id=0;
    
    uint32_t target_threads = 1;

    while (!shutdown) {
        // Adjust number of threads based on pool size
        int sem_value;
        sem_getvalue(global.sem_pool_full, &sem_value);
        if (target_threads < 3) {
            
                printf("WORKING %d!\n", sem_value);
            if (target_threads == 1 && (uint32_t)sem_value > (config.transaction_pool_size * 2 )/ 100) {
                target_threads = 2;
            } 
            else if (target_threads == 2 && (uint32_t)sem_value > (config.transaction_pool_size * 4 )/ 100) {
                target_threads = 3;
            }
            else if (((uint32_t)sem_value < (config.transaction_pool_size * 1 )/ 100)&& target_threads >1) {
                target_threads = 1;
                thread_args->thread_id=0;
            }
        }

        // Create new threads if needed
        while (thread_args->thread_id < target_threads && thread_args->thread_id < 3) {
            if (pthread_create(&validator_threads[thread_args->thread_id], NULL, validator_thread_func, &pipe_validator_fd) != 0) {
                perror("Failed to create validator thread");
                break;
            }
            c_logprintf("[Validator] Created validator thread %d\n", thread_args->thread_id + 1);
            thread_args->thread_id++;
        }
        while (thread_args->thread_id > target_threads) {
            thread_args->thread_id--;
            
            pthread_join(validator_threads[thread_args->thread_id], NULL);
            
            c_logprintf("[Validator] Stopped validator thread %d\n", thread_args->thread_id + 1);
        }
        
    }

    // Signal threads to exit and wait for them
    
    for (uint32_t i = 0; i < thread_args->thread_id; i++) {
        pthread_join(validator_threads[i], NULL);
    }

    // Cleanup
    close(pipe_validator_fd);
    val_cleanup(EXIT_SUCCESS);
}
