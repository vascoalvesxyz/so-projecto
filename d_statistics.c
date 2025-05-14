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
  c_dump_stats()
  shutdown = 1;
}
void s_dump_stats(){
  for(int i=0; i < config.miners_max; i++){
  c_logprintf("Miner %d\n",global.minerm[i].iner_id);
  c_logprintf("Valid_blocks %d\n",global.miner[i].valid_blocks);
  c_logprintf("invalid_blocks %d\n", global.miner[i].invalid_blocks);
  c_logprintf("Avg Time %d\n", global.miner[i].timestamp/miner->total_blocks);
  c_logprintf("Total blocks %d\n", global.miner[i].total_blocks);

  }

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
  global.miners= (MinerBlockInfo*)malloc(sizeof(MinerBlockInfo)*config.miners_max);
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
     global.miners[stats.miner_id]= stats;
    }
    else{
    global.miners[stats.miner_id].valid_blocks+= stats.valid_blocks;
    global.miners[stats.miner_id].invalid_blocks += stats.invalid_blocks;
    global.miners[stats.miner_id].timestamp += stats.timestamp;
    global.miners[stats.miner_id].total_blocks += stats.total_blocks;
    }
    c_dump_stats(); 
  }
  c_dump_stats();
  /* Cleanup after exiting loop */
  c_cleanup_globals();
  c_logputs("[Statistics]: Exited Successfully!\n");
  exit(EXIT_SUCCESS);
}
