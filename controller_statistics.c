#include "controller.h"
#include <string.h>
#include <mqueue.h>
#define QUEUE_NAME "/MinerInfo"

typedef struct Miner_block_info{
      
  char  miner_hash[HASH_SIZE]; // Hash of the previous block
  int valid_blocks;
  int invalid_blocks;
  time_t timestamp;                     // Time when block was created
}Miner_block_info;
void c_stat_main() {
    //Gerar Estatisticas
    // E escrever antes de acabar a simulação (Isto é a simulação acaba e enquanto fecha)
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
    void handle_signit() {
        
        puts("STATISTICS: EXITED SUCCESSFULLY!\n");
       
        c_cleanup();
        exit(EXIT_SUCCESS);
        

    }
    
    
    signal(SIGINT, handle_signit);

    while (1) {
    Miner_block_info info;
    ssize_t bytes = mq_receive(StatsQueue,(char*)&info, sizeof(Miner_block_info), NULL);
    if (bytes > -1){ 
      printf("I have received\n");
    }

  }
}
