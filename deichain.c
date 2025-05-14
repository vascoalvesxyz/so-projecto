#include "deichain.h"
#include <openssl/crypto.h>
#include <stdio.h>

//Vasco Alves 2022228207
//Joao Neto 2023234004

/* Variables to be DUPLICATED */
struct config_t config = {0};
struct global_t global = {0};
_Atomic sig_atomic_t shutdown = 0;
_Atomic sig_atomic_t sigint_received = 0;

/* Private-ish Main Controller Variables */
static pid_t pid_mc;   // miner controller
static pid_t pid_stat; // statistics
static pid_t pid_val;  // transaction validator
 
/* Controller specific functions */
int  c_ctrl_init();
int  c_ctrl_import_config(const char* path, struct config_t *dest);
void c_ctrl_cleanup();
void c_ctrl_handle_sigint();

/* FUNCTIONS */
void c_cleanup_globals() {
  /* Logfile */
  if (global.logfile_fptr) {
    pthread_mutex_lock(&global.logfile_mutex);
    fflush(global.logfile_fptr);
    fclose(global.logfile_fptr);
    global.logfile_fptr = NULL;
    pthread_mutex_unlock(&global.logfile_mutex);
  }

  /* Semaphores - Close all three */
  if (global.sem_pool_empty && global.sem_pool_empty != SEM_FAILED) {
    sem_close(global.sem_pool_empty);
    global.sem_pool_empty = NULL;
  }
  if (global.sem_pool_full && global.sem_pool_full != SEM_FAILED) {
    sem_close(global.sem_pool_full);
    global.sem_pool_full = NULL;
  }
  if (global.sem_pool_mutex && global.sem_pool_mutex != SEM_FAILED) {
    sem_close(global.sem_pool_mutex);
    global.sem_pool_mutex = NULL;
  }

  /* Shared Memory - Pool */
  if (global.shmem_pool_data && global.shmem_pool_data != MAP_FAILED) {
    if (munmap(global.shmem_pool_data, SHMEM_SIZE_POOL)) {
      perror("munmap(shmem_pool_data) failed");
    }
    global.shmem_pool_data = MAP_FAILED;
  }
  if (global.shmem_pool_fd != -1) {
    close(global.shmem_pool_fd);
    global.shmem_pool_fd = -1;
  }

  /* Shared Memory - Blockchain */
  if (global.shmem_blockchain_data && global.shmem_blockchain_data != MAP_FAILED) {
    if (munmap(global.shmem_blockchain_data, SHMEM_SIZE_BLOCKCHAIN)) {
      perror("munmap(shmem_blockchain_data) failed");
    }
    global.shmem_blockchain_data = MAP_FAILED;
  }
  if (global.shmem_blockchain_fd != -1) {
    close(global.shmem_blockchain_fd);
    global.shmem_blockchain_fd = -1;
  }

  /* Message Queue */
  if (global.mq_statistics != (mqd_t)-1) {
    if (mq_close(global.mq_statistics)) {
      perror("mq_close failed");
    }
    global.mq_statistics = (mqd_t)-1;
  }

  /* Mutex */
  if (pthread_mutex_destroy(&global.logfile_mutex) == 0) {
    pthread_mutex_init(&global.logfile_mutex, NULL);
  }

  OPENSSL_cleanup();
}

int c_ctrl_init() {
  /* Clear log file and open it */
  global.logfile_fptr = fopen(FPATH_LOG, "wa");
  if (global.logfile_fptr == NULL) {
    puts("[ERROR] Failed to open log file.");
    return -1;
  }
  /* === Transaction Pool === */
  global.shmem_pool_fd = shm_open(SHMEM_PATH_POOL, O_CREAT | O_RDWR, 0666);
  if (global.shmem_pool_fd < 0) {
    puts("[Controller] Failed to create shared memory for pool.");
    return 0;
  } 

  if (ftruncate(global.shmem_pool_fd, SHMEM_SIZE_POOL) == -1) {
    puts("[Controller] Failed fruncate (pool).");
    return 0;
  }

  global.shmem_pool_data = mmap(NULL, SHMEM_SIZE_POOL, PROT_READ | PROT_WRITE, MAP_SHARED, global.shmem_pool_fd, 0);
  if (global.shmem_pool_data == MAP_FAILED) {
    puts("[Controller] Failed to allocate shared memory for pool.");
    return 0;
  }

  /* Set first transaction as size of pool */
  memset(global.shmem_pool_data, 0, SHMEM_SIZE_POOL);
  global.shmem_pool_data[0].tx.reward = SHMEM_SIZE_POOL;

  /* === Shared Memory:  Blockchain === */
  global.shmem_blockchain_fd = shm_open(SHMEM_PATH_BLOCKCHAIN, O_CREAT | O_RDWR, 0666);
  if (global.shmem_blockchain_fd < 0) {
    puts("[Controller] Failed to create shared memory for blockchain.");
    return 0;
  } 

  if (ftruncate(global.shmem_blockchain_fd, SHMEM_SIZE_BLOCKCHAIN) == -1) {
    puts("[Controller] Failed fruncate (blockchain).");
    return 0;
  }

  global.shmem_blockchain_data = mmap(NULL, SHMEM_SIZE_BLOCKCHAIN, PROT_READ | PROT_WRITE, MAP_SHARED, global.shmem_blockchain_fd, 0);
  if (global.shmem_blockchain_data == NULL) {
    puts("[Controller] Failed to allocate shared memory for blockchain.");
    return 0;
  }
  
  memset(global.shmem_blockchain_data, 0, SHMEM_SIZE_BLOCKCHAIN);
  /* Initialize  semaphores */
  assert(config.pool_size > 0);
  global.sem_pool_empty = sem_open(SEM_POOL_EMPTY, O_CREAT, 0666, config.pool_size); 
  global.sem_pool_full  = sem_open(SEM_POOL_FULL, O_CREAT, 0666, 0); 
  global.sem_pool_mutex = sem_open(SEM_POOL_MUTEX, O_CREAT, 0666, 1); 
  PRINT_SEM("EMPTY", global.sem_pool_empty);
  
  // sem_init(global.sem_pool_empty, 0, config.pool_size);
  // sem_init(global.sem_pool_full,  0, 0);
  // sem_init(global.sem_pool_mutex, 0, 1);

  if (global.sem_pool_empty == SEM_FAILED || global.sem_pool_full == SEM_FAILED || global.sem_pool_mutex == SEM_FAILED) {
    puts("[Controller] Failed to create semaphores");
    return 0;
  }

  struct mq_attr sets = {
    .mq_flags = 0,
    .mq_maxmsg = 10,
    .mq_msgsize = sizeof(MinerBlockInfo),
    .mq_curmsgs = 0
  };

  global.mq_statistics = mq_open(QUEUE_NAME, O_CREAT | O_RDWR, 0666, &sets);
  if (global.mq_statistics == (mqd_t)-1) {
    puts("[Controller] Failed to create message queue");
    return 0;
  }

  /* Create pipe */
  mkfifo(PIPE_VALIDATOR, 0666);

  // OPENSSL_init_crypto(OPENSSL_INIT_NO_ATEXIT, NULL);
  return 1;
}

int c_ctrl_import_config(const char* path, struct config_t *dest) {

  /* Do not read config file if it does not exist */
  FILE *f_config = fopen(path, "r");
  if (f_config == NULL)
    return -1;

  /* Write config to zeros */
  memset((void*) &global, 0, sizeof(struct global_t));
  memset(dest, 0, sizeof(struct config_t));

  int fscanf_retvalue = 0;
  int idx = 0;
  int line = 0;
  char argument[256];
  char value[256];
  char expected_arguments[4][64] = { "NUM_MINERS",  "POOL_SIZE",  "TRANSACTIONS_PER_BLOCK",  "BLOCKCHAIN_BLOCKS"};

  /* Tem que seguir a exacta ordem */
  while (EOF != (fscanf_retvalue = fscanf(f_config, "%s - %s", argument, value)) ) {
    line++;

    if (idx == 5) {
      puts("Imported all items successfully.");
      break;
    }

    /* Check number of arguments */
    if (fscanf_retvalue != 2)  {
      puts("Error reading config: no value in line %d.\n");
      fclose(f_config);
      return 0;
    } 

    /* check value */
    for (size_t i = 0; i < strlen(value); i++) {
      if ( value[i] > '9' || value[i] < '0') {
        printf("Error reading config: invalide value %s in line %d.\n", value, line);
        fclose(f_config);
        return 0;
      }
    }

    if (strcmp(argument, expected_arguments[idx]) == 0) {
      switch (idx) {
        case 0:
          dest->miners_max = atoi(value);
          if (dest->miners_max == 0) {
            fclose(f_config);
            return 0;
          }
          break;
        case 1:
          dest->pool_size = atoi(value);
          break;
        case 2:
          dest->transactions_per_block = atoi(value);
          break;
        case 3:
          dest->blockchain_blocks = atoi(value);
          break;
        default:
          break;
      }

      idx++;
    } else {
      fclose(f_config);
      printf("Error: expected next argument to be %s got %s\n", expected_arguments[idx], argument);
      return 0;
    }
  } 

  fclose(f_config);
  return 1;
}

void c_ctrl_cleanup() {

  /* Await Children Processes. */
  shutdown = 1;
  kill(pid_stat, SIGINT);
  kill(pid_val,  SIGINT);
  kill(pid_mc,   SIGINT);

  // int status;
  // pid_t killed;
  // while ((killed = waitpid(-1, &status, WNOHANG)) > 0);
  #ifdef DEBUG
  puts("[DEBUG] [Controller] Waiting for children to terminate...");
  #endif

  while (wait(NULL) > 0);

  /* Close file descriptors first */
  c_cleanup_globals();

  /* Only controller can unlink */
  sem_unlink(SEM_POOL_EMPTY);
  sem_unlink(SEM_POOL_FULL);
  sem_unlink(SEM_POOL_MUTEX);
  shm_unlink(SHMEM_PATH_POOL);
  shm_unlink(SHMEM_PATH_BLOCKCHAIN);
  mq_unlink(QUEUE_NAME);
  unlink(PIPE_VALIDATOR);
}

void c_ctrl_handle_sigint() {
  puts("SIGNAL: SIGINT - Cleaning up...\n");
  c_ctrl_cleanup();
  exit(EXIT_SUCCESS);
}

int main() {

  /* Import Config and Initialize */
  if (1 != c_ctrl_import_config(FPATH_CONFIG, &config)) {
    puts("[ERR] Shutting down.");
    goto exit_fail;
  }

  if (1 != c_ctrl_init() ) {
    puts("[ERR] Shutting down.");
    goto exit_fail;
  }

  OPENSSL_init_crypto(
        OPENSSL_INIT_NO_LOAD_CONFIG |  // Disable config file loading
        OPENSSL_INIT_NO_ATEXIT,        // Disable auto-cleanup
        NULL
  );
  /* Miner Controller */
  c_logputs("[Controller] Starting miner controller!\n");
  pid_mc = fork();
  if (pid_mc < 0) {
    c_logputs("[Controller] Failed to start miner controller.\n");
    goto exit_fail;
  } else if (pid_mc == 0) {
    c_mc_main(config.miners_max);
    exit(1);
  } 

  /* Validator */
  c_logputs("[Controller] Starting validator controller!\n");
  pid_val = fork();
  if (pid_val < 0) {
    c_logputs("[Controller] Failed to start validator.\n");
    goto exit_fail;
  } else if (pid_val == 0) {
    c_val_main();
    exit(1);
  } 

  /* Statistics */
  c_logputs("[Controller] Starting statistics.\n");
  pid_stat = fork();
  if (pid_stat < 0) {
    c_logputs("[Controller] Failed to start statistics.\n");
    goto exit_fail;
  } else if (pid_stat == 0) {
    c_stat_main();
    exit(1);
  }

  signal(SIGINT, c_ctrl_handle_sigint);
  while (wait(NULL) > 0) { }

  c_ctrl_cleanup();
  exit(EXIT_SUCCESS);

  exit_fail:
  c_ctrl_cleanup();
  exit(EXIT_FAILURE);
}
