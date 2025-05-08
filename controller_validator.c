#include "controller.h"
#include <string.h>
#include <mqueue.h> 
#define QUEUE_NAME "/MinerInfo"
typedef struct Miner_block_info{
      
  char  miner_hash[HASH_SIZE]; // Hash of the previous block
  int valid_blocks;
  int invalid_blocks;
  time_t timestamp;                     // Time when block was created
  int total_blocks;
} Miner_block_info;


int running = 1;

void val_cleanup(int retv) {
    if (pipe_validator_fd < 0) {
        close(pipe_validator_fd);
    }
    
    c_cleanup();

    if (retv == 0) {
        puts("Validator: Exited successfully!\n");
        exit(EXIT_SUCCESS);
    }
    puts("Validator: FAILED!\n");
    exit(EXIT_FAILURE);
}

void val_handle_signit(int sig) {
    if (sig != SIGINT) return;
    shutdown = 1; // Signal threads to exit
    sigint_received = 1; // Signal main thread to exit
}

void c_val_main() {

    struct mq_attr sets = {
        .mq_flags = 0,
        .mq_maxmsg = 10,
        .mq_msgsize = sizeof(Miner_block_info),
        .mq_curmsgs = 0
    };
    StatsQueue = mq_open(QUEUE_NAME, O_CREAT | O_RDWR, 0666, &sets);
    if (StatsQueue == (mqd_t)-1) {
        perror("Queue_open");
        exit(1);
    }

    struct sigaction sa;
    sa.sa_handler = val_handle_signit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    /* Open pipe (not blocking to check errors) */
    pipe_validator_fd = open(PIPE_VALIDATOR, O_RDONLY | O_NONBLOCK);
    if (pipe_validator_fd < 0) {
        puts("Validator: Failed to open FIFO\n");
        val_cleanup(1);
    }
    close(pipe_validator_fd);

    pipe_validator_fd = open(PIPE_VALIDATOR, O_RDONLY );

    unsigned char data_recieved[SHMEM_SIZE_BLOCK];
    TransactionBlock block_recieved = (void*) data_recieved;
    BlockInfo *block_info = (BlockInfo*) block_recieved;

    while (shutdown == 0) {

        unsigned long int count = read(pipe_validator_fd, block_recieved, SHMEM_SIZE_BLOCK);

        printf("Read %ld out of %ld\n", count, SHMEM_SIZE_BLOCK);

        if (count < SHMEM_SIZE_BLOCK) {
            puts("READ ERROR");
            continue;
        }

        printf("[Validator] RECEIVED TRANSACTION ID=");
        for (int i = 0; i < TX_ID_LEN; i++) {
            printf("%02x", block_info->txb_id[i]);
        }
        puts("");
    }

    val_cleanup(0);
}
