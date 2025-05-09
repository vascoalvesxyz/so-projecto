//Vasco Alves 2022228207 Joao Neto 2023234004
#ifndef _CONTROLLER_H_
#define _CONTROLLER_H_
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
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
#include <openssl/sha.h>
#include <mqueue.h> 
#include <strings.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <mqueue.h> 

/*=======================
          MACROS  
  ======================= */
#define FPATH_CONFIG          "config.cfg"
#define FPATH_LOG             "DEIChain_log.txt"
#define SEM_POOL_EMPTY        "/dei_pool_empty"
#define SEM_POOL_FULL         "/dei_pool_full"
#define SEM_POOL_MUTEX        "/dei_pool_mutex"
#define SHMEM_PATH_POOL       "/dei_transaction_pool"
#define SHMEM_PATH_BLOCKCHAIN "/dei_blockchain"
#define PIPE_VALIDATOR        "VALIDATOR_INPUT"
#define QUEUE_NAME            "/MinerInfo"
#define SIZE_BLOCK            sizeof(BlockInfo) + sizeof(Transaction)*g_transactions_per_block
#define SHMEM_SIZE_POOL       sizeof(TransactionPool) * (g_pool_size+1)
#define SHMEM_SIZE_BLOCKCHAIN SIZE_BLOCK * g_blockchain_blocks
#define PIPE_MESSAGE_SIZE     SIZE_BLOCK
#define POW_MAX_OPS 10000000
#define INITIAL_HASH \
  "00006a8e76f31ba74e21a092cca1015a418c9d5f4375e7a4fec676e1d2ec1436"

#define HASH_SIZE 32
#define HASH_STRING_SIZE 65

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

/*=======================
  STRUCTS, TYPES & ENUMS  
  ======================= */
typedef enum { EASY = 1, NORMAL = 2, HARD = 3 } DifficultyLevel;

typedef void* BlockLedger;
typedef unsigned char byte_t;
typedef unsigned char hash_t;
typedef char hashstring_t;

/* Transaction Block Information */
typedef struct {
  hash_t tx_id[HASH_SIZE];  // 32 bytes, Unique transaction ID (e.g., PID + #)
  time_t timestamp;         // 8 bytes, Creation time of the transaction
  int32_t reward;           // 4 bytes, Reward associated with PoW
  float value;              // 4 bytes, Quantity or value transferred
} Transaction;

/* Transaction Block Information */
typedef struct {
  hash_t txb_id[HASH_SIZE];             // 32 bytes, unique id
  hash_t previous_block_hash[HASH_SIZE];// 32 bytes, hash of the previous block
  time_t timestamp;                     // 8 bytes,  Time when block was created
  size_t nonce;                         // 8 bytes,  PoW solution
  Transaction tx_array[]; // flexible array, 8 byte alignment
} BlockInfo; // 80 bytes

typedef void* TransactionBlock;

typedef struct TransactionPool{
  Transaction tx;
  unsigned int age;
  int empty;
} TransactionPool;

typedef struct MinerBlockInfo{
  hash_t miner_hash[HASH_SIZE]; // Hash of the previous block
  int valid_blocks;
  int invalid_blocks;
  time_t timestamp;                     // Time when block was created
  int total_blocks;
} MinerBlockInfo;

typedef struct {
  hash_t hash[HASH_SIZE];
  double elapsed_time;
  int operations;
  int error;
} PoWResult;


/*=======================
     GLOBAL VARIABLES  
  ======================= */
extern char _buf[512];
extern FILE *g_logfile_fptr;
extern pthread_mutex_t g_logfile_mutex;
extern mqd_t StatsQueue;
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
extern BlockInfo *g_shmem_blockchain_data;
/* does this need to be extern? */
extern int g_shmem_pool_fd; 
extern int g_shmem_blockchain_fd;

extern _Atomic sig_atomic_t shutdown;
extern _Atomic sig_atomic_t sigint_received;

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

/* Proof of work */
void c_pow_block_serialize(TransactionBlock input, byte_t *serial);
void c_pow_hash_compute(TransactionBlock input, hash_t *output);
void c_pow_hash_to_string(hash_t *input, hashstring_t *output);
int  c_pow_getmaxreward(TransactionBlock tb);
int  c_pow_checkdifficulty(hash_t *hash, int reward);
int  c_pow_verify_nonce(TransactionBlock tb);
PoWResult c_pow_proofofwork(TransactionBlock *tb);

#endif // !_CONTROLLER_H_
