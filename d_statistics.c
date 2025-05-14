#include "deichain.h"
#include <mqueue.h>
#include <stdlib.h>
#include <string.h>

static MinerBlockInfo* miner_stat_arr;

static void s_dump_stats(){

  for(uint32_t i = 0; i < config.miners_max; i++){
    c_logprintf("Miner %d\n", i);
    c_logprintf("Valid_blocks %d\n",miner_stat_arr[i].valid_blocks);
    c_logprintf("invalid_blocks %d\n", miner_stat_arr[i].invalid_blocks);
    if (miner_stat_arr[i].total_blocks > 0) {

      c_logprintf("Avg Time %lf ms \n", miner_stat_arr[i].pow_time / (miner_stat_arr[i].total_blocks) );
    }
    c_logprintf("Total blocks %d\n", miner_stat_arr[i].total_blocks);
  }

}

static void s_handle_sigint() {
  shutdown = 1;
}

static void s_handle_sigusr() {
  s_dump_stats();
}

void c_stat_main() {

  /* Initialize stat variavles */  
  struct sigaction sa;
  sa.sa_handler = s_handle_sigint;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);

  struct sigaction sa2;
  sa2.sa_handler = s_handle_sigusr;
  sigemptyset(&sa2.sa_mask);
  sa2.sa_flags = 0;
  sigaction(SIGUSR1, &sa2, NULL);

  MinerBlockInfo info = {0};

  size_t arr_size = (config.miners_max+1) * sizeof(MinerBlockInfo);
  miner_stat_arr = malloc(arr_size);
  memset(miner_stat_arr, 0, arr_size);

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
      miner_stat_arr[info.miner_id].pow_time += info.pow_time;
      miner_stat_arr[info.miner_id].total_blocks += info.total_blocks;
    }
  }

  s_dump_stats();

  /* Cleanup after exiting loop */
  free(miner_stat_arr);

  c_cleanup_globals();
  c_logputs("[Statistics]: Exited Successfully!\n");
  exit(EXIT_SUCCESS);
}
