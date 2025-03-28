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

#define FPATH_CONFIG "config.cfg"
#define FPATH_LOG "DEIChain_log.txt"

#define SHMEM_PATH_BLOCKCHAIN "/dei_blockchain"
#define SHMEM_PATH_POOL "/dei_transaction_pool"
#define SHMEM_SIZE_POOL sizeof(Transaction) * g_pool_size

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

static FILE *g_logfile_fptr;
static pthread_mutex_t g_logfile_mutex = PTHREAD_MUTEX_INITIALIZER; 

static int pool_fd = -1; 
static Transaction *pool_data;

static pthread_mutex_t g_miner_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned int g_miners_max = 0;                   // number of miners (number of threads in the miner process)
static unsigned int g_pool_size = 0;                    // number of slots on the transaction pool
static unsigned int g_transactions_per_block = 0;       // number of transactions per block (will affect block size)
static unsigned int g_blockchain_blocks = 0;            // maximum number of blocks that can be saved in the
static unsigned int g_transaction_pool_size = 10000 ;   //

/* FUNCTIONS DEFINITION */
static void logputs(const char* string);
int  c_init();
int  c_import_config();
void c_cleanup();
void c_handle_sigint();

void mc_main();
void* mc_miner_thread(void* id_ptr);
void* validato(void* i);

/* FUNCTIONS DELCARATION */
static void logputs(const char* string) {
    if (string == NULL) return;

    fputs(string, stdout);

    /* REDO THIS SHIT */
    pthread_mutex_lock(&g_logfile_mutex);
    fputs(string, g_logfile_fptr);
    fflush(g_logfile_fptr); // garantir write atomico
    pthread_mutex_unlock(&g_logfile_mutex);

}

/* macro para ter args do printf */
#define logprintf(...)\
        sprintf(_buf, __VA_ARGS__);\
        logputs(_buf)

int c_init() {

    /* Clear log file and open it */
    g_logfile_fptr = fopen(FPATH_LOG, "wa");
    if (g_logfile_fptr == NULL) {
        fputs("Failed to open log file: ", stderr);
        return -1;
    }

    /* Set log file as manual buffer */
    /*setvbuf(g_logfile_fptr, g_logfile_buf, _IOFBF, BUFSIZ);*/

    pool_fd = shm_open(SHMEM_PATH_POOL, O_CREAT | O_RDWR, 0666);
    ftruncate(pool_fd, SHMEM_SIZE_POOL);
    pool_data = mmap(NULL, SHMEM_SIZE_POOL, PROT_READ | PROT_WRITE, MAP_SHARED, pool_fd, 0);

    return 1;
}

int c_import_config(const char* path) {

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

void c_cleanup() {
    pthread_mutex_destroy(&g_miner_mutex);

    if (g_logfile_fptr != NULL) {
        /* é necessário esperar pelo semaforo
         * porque fclose dá flush */
        pthread_mutex_lock(&g_logfile_mutex);
        fclose(g_logfile_fptr);
        pthread_mutex_unlock(&g_logfile_mutex);
    }

    if (pool_fd != -1)
        close(pool_fd);                      

    if (pool_data != MAP_FAILED && pool_data != NULL)
        munmap(pool_data, SHMEM_SIZE_POOL); 
    
    shm_unlink(SHMEM_PATH_POOL);

}

void c_handle_sigint() {
    logputs("\nSIGNAL: SIGINT - Cleaning up...\n");
    c_cleanup();
    /*goto clean_pool;*/
    exit(EXIT_SUCCESS);
}

void mc_main() {
    int mc_miner_thread_id[g_miners_max];
    pthread_t mc_miner_thread_arr[g_miners_max];
    unsigned int i = 0;

    for (i = 0; i < g_miners_max; i++) {
        mc_miner_thread_id[i]=i+1;
        pthread_create(&mc_miner_thread_arr[i], NULL, mc_miner_thread, &mc_miner_thread_id[i]);
    }

    for(i=0; i< g_miners_max; i++){
        pthread_join(mc_miner_thread_arr[i], NULL);
    }
}

void* mc_miner_thread(void* id_ptr) {

    // Cast  void* to int* and dereference
    int id = *( (int *) id_ptr );
    pthread_mutex_lock(&g_miner_mutex);
    numero +=1;
    logprintf("Miner %d: numero=%d\n",id,numero);
    pthread_mutex_unlock(&g_miner_mutex);
    while(1) {

    };
    pthread_exit(NULL);
}

void* validato(void* i){
    logputs("I sure do love to validate stuff \n");
    pthread_exit(NULL);
}



int main() {

    /* Handle signal */
    /* quando devemos ignorar o ctrl-c? */ 
    signal(SIGINT, c_handle_sigint);
    /* handle ctrl-z? */ 
    /*signal(SIGINT, c_handle_sigint);*/

    /* Initialize stuff */
    if (1 != c_init()) {
        c_cleanup();
        exit(EXIT_FAILURE);
    }

    /* Import Config */
    if (1 != c_import_config(FPATH_CONFIG)) {
        c_cleanup();
        exit(EXIT_FAILURE);
    }

    /* Initialize Shared Memory */

    /*pthread_t validator;*/
    /*int trabalhos = g_miners_max;*/

    
    /*int fd_blockchain = shm_open(SHMEM_PATH_BLOCKCHAIN, O_CREAT | O_RDWR, 0666);*/
    /*close(fd_blockchain);*/



    /*pthread_create(&validator, NULL, validato, &trabalhos);*/
    /*pthread_join(validator, NULL);*/

    /* Miner Controller Process */
    pid_t pid = fork();
    if (pid == -1) {
        logputs("Failed to start miner controller.");
        goto exit_1;
    } else if (pid == 0) {
        mc_main();
        goto exit_2;
    }

    while (1) { }

    /* Print statistics before cleaning */
    exit_1:
    logputs("Statistics:\n");
    exit_2:
    c_cleanup();
    return 0;
}

