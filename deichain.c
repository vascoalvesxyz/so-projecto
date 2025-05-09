#include "deichain.h"

//Vasco Alves 2022228207
//Joao Neto 2023234004

#ifdef DEBUG
int _macro_buf;
#endif /* ifdef DEBUG */

static pid_t g_pid_mc   = -1; // miner controller
static pid_t g_pid_stat = -1; // statistics
static pid_t g_pid_val  = -1; // transaction validator

_Atomic sig_atomic_t shutdown = 0;
_Atomic sig_atomic_t sigint_received = 0;

/* Shared Memory */
int g_shmem_pool_fd = -1; 
TransactionPool *g_shmem_pool_data = NULL;
int g_shmem_blockchain_fd = -1;
BlockInfo *g_shmem_blockchain_data = NULL;

int pipe_validator_fd = -1;
unsigned int g_miners_max = 0;                   // number of miners (number of threads in the miner process)
unsigned int g_pool_size = 0;                    // number of slots on the transaction pool
unsigned int g_transactions_per_block = 0;       // number of transactions per block (will affect block size)
unsigned int g_blockchain_blocks = 0;            // maximum number of blocks that can be saved in the
unsigned int g_transaction_pool_size = 10000 ;   // Transactions in POOL

/* Declared external in controller.h */
char _buf[512];
FILE *g_logfile_fptr = NULL;
pthread_mutex_t g_logfile_mutex = PTHREAD_MUTEX_INITIALIZER; 

sem_t *g_sem_pool_empty = NULL;
sem_t *g_sem_pool_full = NULL;
sem_t *g_sem_pool_mutex = NULL;
mqd_t StatsQueue = 0;
/* FUNCTIONS DELCARATION */
void c_logputs(const char* string);
void c_cleanup();
/* Controller functions */
int  c_ctrl_init();
int  c_ctrl_import_config(const char* path);
void c_ctrl_cleanup();
void c_ctrl_handle_sigint();

/* FUNCTIONS */
void c_logputs(const char* string) {
    assert(string != NULL);

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];  // Buffer for "YYYY-MM-DD HH:MM:SS"
    strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S] ", tm_info);

    fputs(timestamp, stdout);
    fputs(string, stdout);

    pthread_mutex_lock(&g_logfile_mutex);
    fputs(timestamp, g_logfile_fptr);
    fputs(string, g_logfile_fptr);
    fflush(g_logfile_fptr); // garantir write atomico
    pthread_mutex_unlock(&g_logfile_mutex);
}

void c_cleanup() {

    /* main controller closes the logfile */
    if (g_logfile_fptr != NULL)
        fclose(g_logfile_fptr);

    /* Every fork will have to close and unmap independently */
    if (g_sem_pool_empty != NULL) sem_close(g_sem_pool_empty);
    if (g_sem_pool_full != NULL) sem_close(g_sem_pool_full);
    if (g_sem_pool_mutex != NULL) sem_close(g_sem_pool_mutex);

    if (g_shmem_pool_fd > -1) 
        close(g_shmem_pool_fd);                      
    if (g_shmem_pool_data != NULL && g_shmem_pool_data != MAP_FAILED)
        munmap(g_shmem_pool_data, SHMEM_SIZE_POOL); 

    if (g_shmem_blockchain_fd > -1) 
        close(g_shmem_blockchain_fd);
    if (g_shmem_blockchain_data != NULL && g_shmem_blockchain_data != MAP_FAILED)
        munmap(g_shmem_blockchain_data,SHMEM_SIZE_BLOCKCHAIN);
    
    if(mq_close(StatsQueue) < 0){
    printf("THIS LINE\n");  
    perror("mq_close");
  }
}

int c_ctrl_init() {
    /* Clear log file and open it */
    g_logfile_fptr = fopen(FPATH_LOG, "wa");
    if (g_logfile_fptr == NULL) {
        fputs("Failed to open log file: ", stderr);
        return -1;
    }

    g_shmem_pool_fd = shm_open(SHMEM_PATH_POOL, O_CREAT | O_RDWR, 0666);
    if (g_shmem_pool_fd < 0) {
        c_logputs("Controller: Failed to create shared memory for pool.");
        return 0;
    } 
if (ftruncate(g_shmem_pool_fd, SHMEM_SIZE_POOL) == -1) {
    perror("ftruncate (pool) failed");
    exit(EXIT_FAILURE);  // or handle the error appropriately
}
    g_shmem_pool_data = mmap(NULL, SHMEM_SIZE_POOL, PROT_READ | PROT_WRITE, MAP_SHARED, g_shmem_pool_fd, 0);
    if (g_shmem_pool_data == MAP_FAILED) {
        c_logputs("Controller: Failed to allocate shared memory for pool.");
        return 0;
    }

    /* Set first transaction as size of pool */
    memset(g_shmem_pool_data, 0, SHMEM_SIZE_POOL);
    g_shmem_pool_data[0].tx.reward = SHMEM_SIZE_POOL;

    g_shmem_blockchain_fd = shm_open(SHMEM_PATH_BLOCKCHAIN, O_CREAT | O_RDWR, 0666);
    if (g_shmem_blockchain_fd < 0) {
        c_logputs("Controller: Failed to create shared memory for blockchain.");
        return 0;
    } 
if (ftruncate(g_shmem_blockchain_fd, SHMEM_SIZE_BLOCKCHAIN) == -1) {
    perror("ftruncate (blockchain) failed");
    exit(EXIT_FAILURE);
}
    g_shmem_blockchain_data = mmap(NULL, SHMEM_SIZE_BLOCKCHAIN, PROT_READ | PROT_WRITE, MAP_SHARED, g_shmem_pool_fd, 0);
    if (g_shmem_blockchain_data == NULL) {
        c_logputs("Controller: Failed to allocate shared memory for blockchain.");
        return 0;
    }

    //Semaphore
    g_sem_pool_empty = sem_open(SEM_POOL_EMPTY, O_CREAT, 0666, g_pool_size); 
    g_sem_pool_full = sem_open(SEM_POOL_FULL, O_CREAT, 0666, 0); 
    g_sem_pool_mutex = sem_open(SEM_POOL_MUTEX, O_CREAT, 0666, 1); 
    sem_init(g_sem_pool_empty, 0, g_pool_size);
    sem_init(g_sem_pool_full,  0, 0);
    sem_init(g_sem_pool_mutex, 0, 1);

    if (g_sem_pool_empty == SEM_FAILED || g_sem_pool_full == SEM_FAILED || g_sem_pool_mutex == SEM_FAILED) {
        c_logputs("[Controller] Failed to create semaphores");
        return 0;
    }
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
    /* Create pipe */
    mkfifo(PIPE_VALIDATOR, 0666);
    
    return 1;
}

int c_ctrl_import_config(const char* path) {

    FILE *f_config = fopen(path, "r");

    /* Do not read config file if it does not exist */
    if (f_config == NULL)
        return -1;

    int fscanf_retvalue = 0;
    int idx = 0;
    int line = 0;
    char argument[256];
    char value[256];

    char expected_arguments[5][64] = { "NUM_MINERS",  "POOL_SIZE",  "TRANSACTIONS_PER_BLOCK",  "BLOCKCHAIN_BLOCKS",  "TRANSACTION_POOL_SIZE"};

    /* Tem que seguir a exacta ordem */
    while (EOF != (fscanf_retvalue = fscanf(f_config, "%s - %s", argument, value)) ) {
        line++;

        if (idx == 5) {
            c_logputs("Imported all items successfully.");
            break;
        }

        /* Check number of arguments */
        if (fscanf_retvalue != 2)  {
            c_logputs("Error reading config: no value in line %d.\n");
            fclose(f_config);
            return 0;
        } 

        /* check value */
        for (size_t i = 0; i < strlen(value); i++) {
            if ( value[i] > '9' || value[i] < '0') {
                c_logprintf("Error reading config: invalide value %s in line %d.\n", value, line);
                fclose(f_config);
                return 0;
            }
        }

        if (strcmp(argument, expected_arguments[idx]) == 0) {
            switch (idx) {
                case 0:
                    g_miners_max = atoi(value);
                    if (g_miners_max == 0) {
                        fclose(f_config);
                        return 0;
                    }
                    break;
                case 1:
                    g_pool_size = atoi(value);
                    break;
                case 2:
                    g_transactions_per_block = atoi(value);
                    break;
                case 3:
                    g_blockchain_blocks = atoi(value);
                    break;
                case 4:
                    g_transaction_pool_size = atoi(value);
                    break;
                default:
                    break;
            }

            idx++;
        } else {
            fclose(f_config);
            c_logprintf("Error: expected next argument to be %s got %s\n", expected_arguments[idx], argument);
            return 0;
        }
    } 

    fclose(f_config);
  return 1;
}

void c_ctrl_cleanup() {

    /* Await Children Processes. */
    kill(g_pid_stat, SIGINT);
    kill(g_pid_val,  SIGINT);
    kill(g_pid_mc,   SIGINT);

    pid_t killed;
    do { killed = wait(NULL); }
    while (killed != -1);
    mq_unlink(QUEUE_NAME);
    /* only controller can unlink */
    sem_unlink(SEM_POOL_EMPTY);
    sem_unlink(SEM_POOL_FULL);
    sem_unlink(SEM_POOL_MUTEX);
    shm_unlink(SHMEM_PATH_POOL);
    shm_unlink(SHMEM_PATH_BLOCKCHAIN);
    unlink(PIPE_VALIDATOR);
    c_cleanup();
}

void c_ctrl_handle_sigint() {
    c_logputs("SIGNAL: SIGINT - Cleaning up...\n");

    c_ctrl_cleanup();
    exit(EXIT_SUCCESS);
}

int main() {

    /* Import Config and Initialize */
    if (1 != c_ctrl_import_config(FPATH_CONFIG)) {
        goto exit_fail;
    }

    if (1 != c_ctrl_init() ) {
        goto exit_fail;
    }

    /* Miner Controller */
    puts("Starting miner controller!");
    g_pid_mc = fork();
    if (g_pid_mc < 0) {
        c_logputs("Failed to start miner controller.\n");
        goto exit_fail;
    } else if (g_pid_mc == 0) {
        c_mc_main(g_miners_max);
    } 

    /* Validator */
    g_pid_val = fork();
    if (g_pid_val < 0) {
        c_logputs("Failed to start validator.\n");
        goto exit_fail;
    } else if (g_pid_val == 0) {
        c_val_main();
    } 

    /* Statistics */
    puts("Starting statistics!");
    g_pid_stat = fork();
    if (g_pid_stat < 0) {
        c_logputs("Failed to start Statistics controller.");
        goto exit_fail;
    } else if (g_pid_stat == 0) {
        c_stat_main();
    }


    /* Statistics */
    signal(SIGINT, c_ctrl_handle_sigint);

    while (wait(NULL) > 0) { }

    c_ctrl_cleanup();
    exit(EXIT_SUCCESS);

    exit_fail:
    c_ctrl_cleanup();
    exit(EXIT_FAILURE);
}
