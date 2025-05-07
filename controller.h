#ifndef _CONTROLLER_H_
#define _CONTROLLER_H_
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include "pow.h"


/*=======================
          MACROS  
  ======================= */
#define FPATH_CONFIG            "config.cfg"
#define FPATH_LOG               "DEIChain_log.txt"
#define SEM_POOL_EMPTY          "/dei_pool_empty"
#define SEM_POOL_FULL           "/dei_pool_full"
#define SEM_POOL_MUTEX          "/dei_pool_mutex"
#define SHMEM_PATH_POOL         "/dei_transaction_pool"
#define SHMEM_PATH_BLOCKCHAIN   "/dei_blockchain"
#define PIPE_VALIDATOR          "VALIDATOR_INPUT"
#define SHMEM_SIZE_POOL       sizeof(Transaction) * (g_pool_size+1)
#define SHMEM_SIZE_BLOCK      sizeof(Transaction) * g_transactions_per_block
#define SHMEM_SIZE_BLOCKCHAIN sizeof(Transaction)*  g_transactions_per_block * g_blockchain_blocks

/* TODO: Replace sprintf with snprintf */
#define c_logprintf(...)\
        sprintf(_buf, __VA_ARGS__);\
        c_logputs(_buf)

/* Print semaphore macro (debug only) */
#ifdef DEBUG
#define PRINT_SEM(NAME, sem)\
    if (sem_getvalue(sem, &_macro_buf) == 0) {\
        printf("[DEBUG] [TxGen] SEMAPHORE %s = %d\n", NAME, _macro_buf);\
    } else { perror("sem_getvalue failed"); }
#endif /* ifdef DEBUG */

typedef struct TransactionPool{
  Transaction tx;
  unsigned int age;
  int empty;
}TransactionPool;

/*=======================
     GLOBAL VARIABLES  
  ======================= */
extern char _buf[512];
extern FILE *g_logfile_fptr;
extern pthread_mutex_t g_logfile_mutex;
/* Configuration */
extern unsigned int g_miners_max;                   // number of miners (number of threads in the miner process)
extern unsigned int g_pool_size;                    // number of slots on the transaction pool
extern unsigned int g_transactions_per_block;       // number of transactions per block (will affect block size)
extern unsigned int g_blockchain_blocks;            // maximum number of blocks that can be saved in the
extern unsigned int g_transaction_pool_size;        // Transactions in POOL

/* pipes */
extern int pipe_validator_fd;

/* semaphores */
extern sem_t *g_sem_pool_empty; // counts how many empty slots
extern sem_t *g_sem_pool_full;  // transaction pool is full
extern sem_t *g_sem_pool_mutex; // transaction pool mutex 

/* shared memory */
extern TransactionPool *g_shmem_pool_data;
extern TransactionBlock *g_shmem_blockchain_data;
/* does this need to be extern? */
extern int g_shmem_pool_fd; 
extern int g_shmem_blockchain_fd;

/*=======================
   FUNCTION DEFINITIONS  
  ======================= */
void c_logputs(const char* string);
void c_cleanup();       // general cleanup function
/* Controller */
int  c_ctrl_init();     // intialize controller       
int  c_ctrl_import_config(const char* path);
void c_ctrl_cleanup(); // controller specific cleanup
void c_ctrl_handle_sigint();
/* Miner Controller */
void c_mc_main(unsigned int miners_max);
/* Validator */
void c_val_main();
/* Statistics */
void c_stat_main();

#endif // !_CONTROLLER_H_
