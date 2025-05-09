#include "deichain.h"

void c_stat_main() {
    //Gerar Estatisticas
    // E escrever antes de acabar a simulação (Isto é a simulação acaba e enquanto fecha)
    struct mq_attr sets = {
        .mq_flags = 0,
     .mq_maxmsg = 10,
        .mq_msgsize = sizeof(MinerBlockInfo),
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
    MinerBlockInfo info;
    ssize_t bytes = mq_receive(StatsQueue,(char*)&info, sizeof(MinerBlockInfo), NULL);
    if (bytes > -1){ 
      printf("I have received\n");
    }

  }
}
