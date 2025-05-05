#ifndef _CONTROLLER_H_
#define _CONTROLLER_H_

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

#define FPATH_LOG "DEIChain_log.txt"
#define SHMEM_PATH_POOL "/dei_transaction_pool"
#define SHMEM_PATH_BLOCKCHAIN "/dei_blockchain"
#define SEM_POOL_EMPTY "/dei_pool_empty"
#define SEM_POOL_FULL "/dei_pool_full"
#define SEM_POOL_MUTEX "/dei_pool_mutex"

/* log related variables will be accessed by subprocesses */
extern char _buf[512];
extern FILE *g_logfile_fptr;
extern pthread_mutex_t g_logfile_mutex; 

void c_logputs(const char* string);
#define c_logprintf(...)\
        sprintf(_buf, __VA_ARGS__);\
        c_logputs(_buf)

void c_cleanup();
int  c_ctrl_init();            
int  c_ctrl_import_config(const char* path);
void c_ctrl_cleanup();
void c_ctrl_handle_sigint();

void c_val_main();
void c_stat_main();
void c_mc_main(unsigned int miners_max);

#endif // !_CONTROLLER_H_
