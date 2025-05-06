//Vasco Alves 2022228207 Joao Neto 2023234004
#include <strings.h>
#include <signal.h>

#include "controller.h"
#include "transaction.h"

typedef struct MinerThreadArgs {
    pthread_mutex_t *mutex;
    int id;
} MinerThreadArgs;

unsigned int i = 0, created_threads = 0;
pthread_mutex_t miner_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_t *mc_threads;
MinerThreadArgs *mc_args;

void* miner_thread(void* mc_args_ptr) {
    int pool_size = g_pool_size;
    Transaction *pool_ptr = g_shmem_pool_data; 

    MinerThreadArgs mc_args = *( (MinerThreadArgs*) mc_args_ptr );
    while (1) {
        c_logprintf("[Miner Thread %d] Waiting...\n", mc_args.id);

        sem_wait(g_sem_pool_full);
        sem_wait(g_sem_pool_mutex);
        for (unsigned i = pool_size-1; i > 1 ; i--) {
            /* TODO: Mine Transaction */
            if (pool_ptr[i].id_self != 0) {
                pool_ptr[i].id_self = 0;
                c_logprintf("[Miner Thread %d] Mining transaction in slot: %d\n", mc_args.id, i);
                break;
            }
        }
        sem_post(g_sem_pool_mutex);
        sem_post(g_sem_pool_empty);
    }

    pthread_exit(NULL);
}

void mc_cleanup() {
    /* wait for miner threads to be done */
    for(i = 0; i < created_threads; i++){
        pthread_join(mc_threads[i], NULL);
    }
    pthread_mutex_destroy(&miner_mutex);

    free(mc_threads);
    free(mc_args);

    c_logputs("Miner Controller: Exited successfully!\n");
    c_cleanup();
    exit(EXIT_SUCCESS);
}

/* Function Declarations */
void c_mc_main(unsigned int miners_max) {

    mc_threads = malloc(sizeof(pthread_t) * miners_max );
    mc_args = malloc(sizeof(struct MinerThreadArgs) * miners_max );

    /* Create Miner Threads */
    for (i = 0; i < miners_max; i++, created_threads++) {
        mc_args[i] = (MinerThreadArgs) {&miner_mutex, i+1};
        puts("Creating thread");
        pthread_create(&mc_threads[i], NULL, miner_thread, (void*) &mc_args[i]);
    }

    signal(SIGINT, mc_cleanup);

    mc_cleanup(0);
}
