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
#include <stdarg.h>

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
#define SIZE_BLOCK            sizeof(BlockInfo) + sizeof(Transaction)*config.transactions_per_block
#define SHMEM_SIZE_POOL       sizeof(TransactionPool) * (config.pool_size+1)
#define SHMEM_SIZE_BLOCKCHAIN SIZE_BLOCK * config.blockchain_blocks
#define PIPE_MESSAGE_SIZE     SIZE_BLOCK
#define POW_MAX_OPS 10000000
#define INITIAL_HASH \
"00006a8e76f31ba74e21a092cca1015a418c9d5f4375e7a4fec676e1d2ec1436"

#define HASH_SIZE 32
#define HASH_STRING_SIZE 65

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
struct config_t {
  uint32_t miners_max;                   // number of miners (number of threads in the miner process)
  uint32_t pool_size;                    // number of slots on the transaction pool
  uint32_t transactions_per_block;       // number of transactions per block (will affect block size)
  uint32_t blockchain_blocks;            // maximum number of blocks that can be saved in the
  uint32_t transaction_pool_size;        // Transactions in POOL
};

struct global_t {
  mqd_t mq_statistics;
  /* Shared Memory */
  TransactionPool *shmem_pool_data;       // 8 bytes
  BlockInfo       *shmem_blockchain_data; // 8 bytes
  /* File descriptors */
  FILE *logfile_fptr;                     // 8 bytes
  sem_t *sem_pool_empty; // 8 bytes,  counts how many empty slots
  sem_t *sem_pool_full;  // 8 bytes,  transaction pool is full
  sem_t *sem_pool_mutex; // 8 bytes,  transaction pool mutex 
  pthread_mutex_t logfile_mutex; // ? bytes
  int shmem_pool_fd;        // 4 bytes 
  int shmem_blockchain_fd;  // 4 bytes 
};

extern struct config_t config;
extern struct global_t global;
extern _Atomic sig_atomic_t shutdown;
extern _Atomic sig_atomic_t sigint_received;

/*=======================
   FUNCTION DEFINITIONS  
  ======================= */
void c_cleanup_globals();

/* Controller */
int  c_ctrl_init();     // intialize both controller and all global vaiables       
int  c_ctrl_import_config(const char* path, struct config_t *dest);
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

/*=======================
   FUNCTION DEFINITIONS  
  ======================= */

static inline void c_logputs(const char* string) {
  assert(string);

  time_t now = time(NULL);
  struct tm tm_info;
  localtime_r(&now, &tm_info);  // thread-safe time conversion

  char timestamp[32];
  strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S] ", &tm_info);

  // pthread_mutex_lock(&g_logfile_mutex);

  fputs(timestamp,  stdout);
  fputs(string,     stdout);
  fflush(stdout);

  if (global.logfile_fptr) {
    fputs(timestamp,  global.logfile_fptr);
    fputs(string,     global.logfile_fptr);
    fflush(global.logfile_fptr);
  }

  // pthread_mutex_unlock(&g_logfile_mutex);
}

static inline void c_logprintf(const char *fmt, ...) {
  char _buf[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(_buf, sizeof(_buf), fmt, args);
  va_end(args);
  c_logputs(_buf);
}

#ifdef DEBUG
static inline void print_semaphore(const char *name, sem_t *sem) {
  int value;
  if (sem_getvalue(sem, &value) == 0) {
    printf("[DEBUG] SEMAPHORE %s = %d\n", name, value);
  } else {
    perror("sem_getvalue failed");
  }
}
#define PRINT_SEM(NAME, SEM) print_semaphore(NAME, SEM)
#endif

#endif // !_CONTROLLER_H_
