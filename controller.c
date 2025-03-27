#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#define FPATH_CONFIG "config.cfg"
#define FPATH_LOG "DEIChain_log.txt"

// Só mutex? só podem entrar um de cada
// Shared memory como é?
// Realease está bem?

/* GLOBAL VARIABLES */
int numero=0;
sem_t empty;

static pthread_mutex_t g_mutex_miner = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_mutex_logfile = PTHREAD_MUTEX_INITIALIZER;

static unsigned int g_miners_max = 0;                   // number of miners (number of threads in the miner process)
static unsigned int g_pool_size = 0;                    // number of slots on the transaction pool
static unsigned int g_transactions_per_block = 0;       // number of transactions per block (will affect block size)
static unsigned int g_blockchain_blocks = 0;            // maximum number of blocks that can be saved in the
static unsigned int g_transaction_pool_size = 10000 ;   //

/* FUNCTIONS DEFINITION */
static void helper_print_and_log(const char* string);
void c_import_config();
void c_cleanup();
void c_handle_sigint(int sig);
void* miner_thread(void* i);
void* validato(void* i);

/* FUNCTIONS DELCARATION */
static void helper_print_and_log(const char* string) {
    if (string == NULL) return;

    printf("%s\n", string);

    FILE *f_log = fopen(FPATH_LOG, "a");
    if (f_log != NULL) {
        pthread_mutex_lock(&g_mutex_logfile);
        fprintf(f_log, "%s\n", string);
        pthread_mutex_unlock(&g_mutex_logfile);
        fclose(f_log);
    }

}

void c_import_config(const char* path) {

    FILE *f_config = fopen(path, "r");

    /* Do not read config file if it does not exist */
    if (f_config == NULL) return;
    
    int line = 0;
    char argument[256];
    char value[256];

    while (2 == fscanf(f_config, "%s - %s", argument, value)) {
        line++;
        /* if value is not a number ignore it 
         * because atoi would convert to 0 */
        if ( 0 != isalpha(value) ) {
            printf("Error reading config: invalide value %s in line %d.\n", value, line);
            break;
        } 

        if (strcmp(argument, "NUM_MINERS") == 0) {
            g_miners_max = atoi(value);
            continue;
        }
        if (strcmp(argument, "POOL_SIZE") == 0) {
            g_pool_size = atoi(value);
            continue;
        }
        if (strcmp(argument, "TRANSACTIONS_PER_BLOCK") == 0) {
            g_transactions_per_block = atoi(value);
            continue;
        }
        if (strcmp(argument, "BLOCKCHAIN_BLOCKS") == 0) {
            g_blockchain_blocks = atoi(value);
            continue;
        }
        if (strcmp(argument, "TRANSACTION_POOL_SIZE") == 0) {

            g_transaction_pool_size = atoi(value) ;
            printf("g_transaction_pool_size = \n");
            continue;
        }

    }

    fclose(f_config);
}

void c_cleanup() {
    pthread_mutex_destroy(&g_mutex_miner);
    pthread_mutex_destroy(&g_mutex_logfile);
}

void c_handle_sigint(int sig) {
    printf("\nCleaning up...\n");
    c_cleanup();

    printf("Statistics:\n");
    printf("Bitches:0\n");
    printf("Genshin Impact female characters in your account: 58");
    exit(EXIT_SUCCESS);  // Exit gracefully
}

void* miner_thread(void* i) {
    int w_id=*((int *)i);
    pthread_mutex_lock(&g_mutex_miner);
    printf("Miner %d: numero=%d\n",w_id,numero);
    numero +=1;
    pthread_mutex_unlock(&g_mutex_miner);
    pthread_exit(NULL);
}

void* validato(void* i){
    printf("I sure do love to validate stuff \n");
    pthread_exit(NULL);
}


int main() {

    /* Handle signal */
    signal(SIGINT, c_handle_sigint);

    /* Import Config */
    c_import_config(FPATH_CONFIG);

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

    while(1){

    }

    c_cleanup();
    return 0;
}

