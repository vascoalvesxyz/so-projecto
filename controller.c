#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#define FPATH_CONFIG "config.cfg"
#define FPATH_LOG "DEIChain_log.txt"

#define SHMEM_PATH_BLOCKCHAIN "/dei_blockchain"
#define SHMEM_PATH_POOL "/dei_transaction_pool"
#define SHMEM_SIZE_POOL sizeof(Transaction) * g_pool_size

typedef struct MinerThreadArgs {
    pthread_mutex_t *mutex;
    int id;
} MinerThreadArgs;

typedef struct Transaction {
    uint32_t id_self;       // 4 bytes
    uint32_t id_sender;     // 4 bytes
    uint32_t id_reciever;   // 4 bytes
    uint32_t timestamp;     // 4 bytes (beware of 2032)
    uint8_t reward;         // 1 bytes
    uint8_t value;          // 1 bytes 
} Transaction;

/* GLOBAL VARIABLES */
int numero=0;
static char _buf[512];              // common buffer for many miscellaenous operations

/* Log file */
static FILE *g_logfile_fptr = NULL;
static pthread_mutex_t g_logfile_mutex = PTHREAD_MUTEX_INITIALIZER; 

/* Shared Memory */
static int g_shmem_pool_fd = -1; 
static Transaction *g_shmem_pool_data = NULL;

/* Process that we will generate */
static pid_t g_pid_mc   = -1; // miner controller
static pid_t g_pid_stat = -1; // statistics
static pid_t g_pid_val  = -1; // transaction validator

/* Configuration Variables */
static unsigned int g_miners_max = 0;                   // number of miners (number of threads in the miner process)
static unsigned int g_pool_size = 0;                    // number of slots on the transaction pool
static unsigned int g_transactions_per_block = 0;       // number of transactions per block (will affect block size)
static unsigned int g_blockchain_blocks = 0;            // maximum number of blocks that can be saved in the
static unsigned int g_transaction_pool_size = 10000 ;   //

/* FUNCTIONS DEFINITION */
static void logputs(const char* string);

void cleanup();

/* Controller */
int  ctrler_init();               // Initialize global variables;
int  ctrler_import_config();     // import configuration
void ctrler_cleanup();           // destroy global variables
void ctrler_handle_sigint();

/* Miner Controller */
void  mc_main();
void* mc_miner_thread(void* args);

/* Validator */
void* validato(void* i);

/* FUNCTIONS DELCARATION */
static void logputs(const char* string) {
    if (string == NULL) return;

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];  // Buffer for "YYYY-MM-DD HH:MM:SS"
    strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S] ", tm_info);

    fputs(timestamp, stdout);
    fputs(string, stdout);

    pthread_mutex_lock(&g_logfile_mutex);
    /*fputs(timestamp, g_logfile_fptr);*/
    fputs(string, g_logfile_fptr);
    fflush(g_logfile_fptr); // garantir write atomico
    pthread_mutex_unlock(&g_logfile_mutex);
}

/* macro para ter args do printf */
#define logprintf(...)\
        sprintf(_buf, __VA_ARGS__);\
        logputs(_buf)

void cleanup() {

    /* main controller closes the logfile */
    if (g_logfile_fptr != NULL) {
        pthread_mutex_lock(&g_logfile_mutex);
        fclose(g_logfile_fptr);
        pthread_mutex_unlock(&g_logfile_mutex);
    }
    /* Every fork will have to close and unmap independently */
    if (g_shmem_pool_fd > -1) 
        close(g_shmem_pool_fd);                      
    if (g_shmem_pool_data != NULL && g_shmem_pool_data != MAP_FAILED)
        munmap(g_shmem_pool_data, SHMEM_SIZE_POOL); 
}

int ctrler_init() {
    /* Clear log file and open it */
    g_logfile_fptr = fopen(FPATH_LOG, "wa");
    if (g_logfile_fptr == NULL) {
        fputs("Failed to open log file: ", stderr);
        return -1;
    }
    g_shmem_pool_fd = shm_open(SHMEM_PATH_POOL, O_CREAT | O_RDWR, 0666);
    ftruncate(g_shmem_pool_fd, SHMEM_SIZE_POOL);
    g_shmem_pool_data = mmap(NULL, SHMEM_SIZE_POOL, PROT_READ | PROT_WRITE, MAP_SHARED, g_shmem_pool_fd, 0);
    return 1;
}

int ctrler_import_config(const char* path) {

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
            logputs("Imported all items successfully.");
            break;
        }

        /* Check number of arguments */
        if (fscanf_retvalue != 2)  {
            logputs("Error reading config: no value in line %d.\n");
            fclose(f_config);
            return 0;
        } 

        /* check value */
        for (size_t i = 0; i < strlen(value); i++) {
            if ( value[i] > '9' || value[i] < '0') {
                logprintf("Error reading config: invalide value %s in line %d.\n", value, line);
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
            logprintf("Error: expected next argument to be %s got %s\n", expected_arguments[idx], argument);
            return 0;
        }
    } 

    fclose(f_config);
    return 1;
}

void ctrler_cleanup() {

    cleanup();


    /* only controller can unlink */
    shm_unlink(SHMEM_PATH_POOL);
}

void ctrler_handle_sigint() {
    logputs("SIGNAL: SIGINT - Cleaning up...\n");
    ctrler_cleanup();
    exit(EXIT_SUCCESS);
}

void mc_main() {
    MinerThreadArgs args[g_miners_max];
    pthread_t mc_miner_thread_arr[g_miners_max];

    unsigned int i = 0;
    pthread_mutex_t miner_mutex = PTHREAD_MUTEX_INITIALIZER;

    for (i = 0; i < g_miners_max; i++) {
        args[i] = (MinerThreadArgs) {&miner_mutex, i+1};
        pthread_create(&mc_miner_thread_arr[i], NULL, mc_miner_thread, (void*) &args[i]);
    }

    /* wait for miner threads to be done */
    for(i=0; i< g_miners_max; i++){
        pthread_join(mc_miner_thread_arr[i], NULL);
    }

    pthread_mutex_destroy(&miner_mutex);
    logputs("Miner Controller: Exited successfully!");

    cleanup();
    exit(EXIT_SUCCESS);
}

void* mc_miner_thread(void* args_ptr) {
    MinerThreadArgs args = *( (MinerThreadArgs*) args_ptr );
    pthread_mutex_lock(args.mutex);
    numero +=1;
    logprintf("miner %d: numero=%d\n",args.id,numero);
    pthread_mutex_unlock(args.mutex);
    pthread_exit(NULL);
}

void* validato(void* i){
    logputs("I sure do love to validate stuff \n");
    pthread_exit(NULL);
}

int main() {

    /* Import Config and Initialize */
    if (1 != ctrler_init() || 1 != ctrler_import_config(FPATH_CONFIG) ) {
        goto exit_fail;
    }

    /* Miner Controller */
    g_pid_mc = fork();
    if (g_pid_mc < 0) {
        logputs("Failed to start miner controller.\n");
        goto exit_fail;
    } else if (g_pid_mc == 0) {
        mc_main();
    } 

    signal(SIGINT, ctrler_handle_sigint);

    while (1) { }

    exit_sucess:
    ctrler_cleanup();
    exit(EXIT_SUCCESS);

    exit_fail:
    ctrler_cleanup();
    exit(EXIT_FAILURE);
}
