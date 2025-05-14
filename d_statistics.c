#include "deichain.h"
#include <mqueue.h>

/* Anonymous struct for passing variables
 * from the main functions' stack */
typedef struct {
  struct mq_attr mq_sets;
} stat_vars_t;

static void s_init(stat_vars_t *vars); // Initialize variavles
static void s_handle_sigint();      

static void s_init(stat_vars_t *vars) {

  struct mq_attr mq_settings = {
    .mq_flags = 0,
    .mq_maxmsg = 10,
    .mq_msgsize = sizeof(MinerBlockInfo),
    .mq_curmsgs = 0
  };

  vars->mq_sets = mq_settings;
}

static void s_handle_sigint() {
  shutdown = 1;
}

void c_stat_main() {

  /* Initialize stat variavles */  
  stat_vars_t vars;
  s_init(&vars);

  struct sigaction sa;
  sa.sa_handler = s_handle_sigint;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);

  global.mq_statistics = mq_open(QUEUE_NAME, O_CREAT | O_RDWR, 0666, &vars.mq_sets);

  if (global.mq_statistics == (mqd_t)-1) {
    puts("[Statistics] mq_open failed ");
    exit(EXIT_FAILURE);
  }

  MinerBlockInfo info;
  MinerBlockInfo* miners= (MinerBlockInfo*)malloc(sizeof(MinerBlockInfo)*config.miners_max);
  while (shutdown == 0) {
    if (mq_receive(global.mq_statistics, (char*)&info, sizeof(MinerBlockInfo), NULL) < 0) {
      if (shutdown != 0) break;

      if (errno == EINTR) {
        continue;
      }

      puts("mq_receive");
    }
    MinerBlockInfo stats = (MinerBlockInfo) info;
    
    if( miners[stats.miner_id].miner_id != stats.miner_id){
     miners[stats.miner_id]= stats;
    }
    else{
    miners[stats.miner_id].valid_blocks+= stats.valid_blocks;
    miners[stats.miner_id].invalid_blocks += stats.invalid_blocks;
    miners[stats.miner_id].timestamp += stats.timestamp;
    miners[stats.miner_id].total_blocks += stats.total_blocks;
    }
    printf("I have received %d \n", stats.miner_id);
    printf("valid_blocks = %d\n", miners[stats.miner_id].valid_blocks);
    printf("invalid_blocks = %d\n", miners[stats.miner_id].invalid_blocks);
    printf("total_blocks = %d\n", miners[stats.miner_id].total_blocks);
  }

  /* Cleanup after exiting loop */
  c_cleanup_globals();
  c_logputs("[Statistics]: Exited Successfully!\n");
  exit(EXIT_SUCCESS);
}
