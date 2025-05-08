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
}Miner_block_info;


int running = 1;

void val_exit(int retv) {

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

void val_handle_signit() {
    val_exit(0);
}

void c_val_main() {

    /* Open pipe (not blocking to check errors) */
    pipe_validator_fd = open(PIPE_VALIDATOR, O_RDONLY | O_NONBLOCK);
    if (pipe_validator_fd < 0) {
        puts("Validator: Failed to open FIFO\n");
        val_exit(1);
    }
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
    close(pipe_validator_fd);
    signal(SIGINT, val_handle_signit);

    pipe_validator_fd = open(PIPE_VALIDATOR, O_RDONLY);

    TransactionPool transaction_recieved;

    while (1) {
        ssize_t count = read(pipe_validator_fd, (char* )&transaction_recieved, sizeof(TransactionPool));


        if (count < 0) {
            puts("READ ERROR");
            val_exit(1);
        } else if (count == 0) {
            continue;
        }
        printf("[Validator] RECEIVED TRANSACTION ID=");
        for (int i = 0; i < TX_ID_LEN; i++) {
            printf("%02x", transaction_recieved.tx.tx_id[i]);
        }
    }

    val_exit(0);
}
