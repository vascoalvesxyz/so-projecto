#include "deichain.h"
#include <mqueue.h>
#include <stdlib.h>

static MinerBlockInfo* miner_stat_arr;

static void s_handle_sigint() {
  shutdown = 1;
}

void c_stat_main() {

  /* Initialize stat variavles */  
  struct sigaction sa;
  sa.sa_handler = s_handle_sigint;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);

  MinerBlockInfo info = {0};
  miner_stat_arr = (MinerBlockInfo*) calloc(config.miners_max+1, sizeof(MinerBlockInfo));
  memset(miner_stat_arr, 0, (config.miners_max+1) * sizeof(MinerBlockInfo));

  while (shutdown == 0) {

    /* Waits for message queue */
    if (mq_receive(global.mq_statistics, (char*)&info, sizeof(MinerBlockInfo), NULL) < 0) {
      c_logputs("[Statistics] Receive failed.\n");
      continue;
    } else if (shutdown != 0) {
      break;
    } 

    c_logputs("[Statistics] Received message.\n");

    if( miner_stat_arr[info.miner_id].miner_id != info.miner_id){
      miner_stat_arr[info.miner_id]= info;
    }
    else{
      miner_stat_arr[info.miner_id].valid_blocks += info.valid_blocks;
      miner_stat_arr[info.miner_id].invalid_blocks += info.invalid_blocks;
      miner_stat_arr[info.miner_id].timestamp += info.timestamp;
      miner_stat_arr[info.miner_id].total_blocks += info.total_blocks;
    }

    #ifdef DEBUG
    printf("I have received %d \n", info.miner_id);
    printf("valid_blocks = %d\n", miner_stat_arr[info.miner_id].valid_blocks);
    printf("invalid_blocks = %d\n", miner_stat_arr[info.miner_id].invalid_blocks);
    printf("total_blocks = %d\n", miner_stat_arr[info.miner_id].total_blocks);
    #endif /* ifdef DEBUG */
  }

  /* Cleanup after exiting loop */
  free(miner_stat_arr);
  c_cleanup_globals();
  c_logputs("[Statistics]: Exited Successfully!\n");
  exit(EXIT_SUCCESS);
}
