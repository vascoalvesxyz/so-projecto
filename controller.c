#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdint.h>

#define FPATH_CONFIG "config.cfg"
#define FPATH_LOG "DEIChain_log.txt"

/* GLOBAL VARIABLES */
int numero=0;
sem_t empty;

static pthread_mutex_t g_mutex_miner = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_mutex_logfile = PTHREAD_MUTEX_INITIALIZER;

static char _buf[BUFSIZ];
static unsigned int g_miners_max = 0;                   // number of miners (number of threads in the miner process)
static unsigned int g_pool_size = 0;                    // number of slots on the transaction pool
static unsigned int g_transactions_per_block = 0;       // number of transactions per block (will affect block size)
static unsigned int g_blockchain_blocks = 0;            // maximum number of blocks that can be saved in the
static unsigned int g_transaction_pool_size = 10000 ;   //


struct Transaction {
    uint32_t id_self;       // 4 bytes
    uint32_t id_sender;     // 4 bytes
    uint32_t id_reciever;   // 4 bytes
    uint32_t timestamp;     // 4 bytes
    uint8_t reward;         // 1 bytes
    uint8_t value;          // 1 bytes 
};

struct TransactionBlock {
};

/* FUNCTIONS DEFINITION */
static void logputs(const char* string);
int c_import_config();
void c_cleanup();
void c_handle_sigint(int sig);
void* miner_thread(void* i);
void* validato(void* i);

/* FUNCTIONS DELCARATION */
static void logputs(const char* string) {
    if (string == NULL) return;

    fputs(string, stdout);

    FILE *f_log = fopen(FPATH_LOG, "a");
    if (f_log != NULL) {
        pthread_mutex_lock(&g_mutex_logfile);
        fputs(string, f_log);
        pthread_mutex_unlock(&g_mutex_logfile);
        fclose(f_log);
    }

}

/* macro para ter args do printf */
#define logprintf(...)\
        sprintf(_buf, __VA_ARGS__);\
        logputs(_buf)

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
        for (int i = 0; i < strlen(value); i++) {
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
    pthread_mutex_destroy(&g_mutex_miner);
    pthread_mutex_destroy(&g_mutex_logfile);
    logputs("Statistics:\n");
}

void c_handle_sigint(int sig) {
    logputs("SIGNAL: SIGINT - Cleaning up...\n");
    c_cleanup();
    exit(EXIT_SUCCESS);
}

void* miner_thread(void* i) {
    int w_id=*((int *)i);
    pthread_mutex_lock(&g_mutex_miner);
    logprintf("Miner %d: numero=%d\n",w_id,numero);
    numero +=1;
    pthread_mutex_unlock(&g_mutex_miner);
    logprintf("Miner %d: numero=%d\n",w_id,numero);
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

    FILE *f_log = fopen(FPATH_LOG, "w");
    if (f_log == NULL) {
        fputs("Failed to open log file: ", stderr);
        c_cleanup();
        exit(EXIT_FAILURE);
    }
    fclose(f_log);

    /* Import Config */
    if (1 != c_import_config(FPATH_CONFIG)) {
        c_cleanup();
        exit(EXIT_FAILURE);
    }

    /*key_t pool_shmem_key = ftok();*/
    /*key_t ledger_shmem_key = ftok();*/



    int i = 0;
    pthread_t validator;
    int trabalhos = g_miners_max;
    int miner_thread_id[g_miners_max];
    pthread_t miner_thread_arr[g_miners_max];

    if (g_miners_max == 0) {
        puts("Please define NUM_MINERS config file.");
        c_cleanup();
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < g_miners_max; i++) {
        miner_thread_id[i]=i+1;
        pthread_create(&miner_thread_arr[i], NULL, miner_thread, &miner_thread_id[i]);
    }

    pthread_create(&validator, NULL, validato, &trabalhos);

    for(i=0; i< g_miners_max; i++){
        pthread_join(miner_thread_arr[i], NULL);
    }
    pthread_join(validator, NULL);

    while(1) {

    }

    c_cleanup();
    return 0;
}

