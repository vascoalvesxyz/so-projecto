//Vasco Alves 2022228207 Joao Neto 2023234004
#include <strings.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>


#include "controller.h"
#include "transaction.h"

#define DEBUG

typedef struct MinerThreadArgs {
    int id;
} MinerThreadArgs;

unsigned int i = 0, created_threads = 0;
pthread_t *mc_threads;
int *id_array;

volatile sig_atomic_t shutdown = 0;
volatile sig_atomic_t sigint_received = 0;


void* miner_thread(void* id_ptr) {
    int pool_size = g_pool_size;
    Transaction *pool_ptr = g_shmem_pool_data; 

    int id = *( (int*) id_ptr );




    while (shutdown == 0) {

        if (sem_trywait(g_sem_pool_full) == 0) {
            // Successfully decremented semaphore
            printf("[Miner Thread %d] Waiting...\n", id);

            #ifdef DEBUG
            puts("[Miner controller] Finished waiting for pool full");
            #endif
            if (shutdown == 1)
                break;

            #ifdef DEBUG
            puts("[Miner controller] Waiting for mutex");
            #endif
            sem_wait(g_sem_pool_mutex);
            if (shutdown == 1)
                break;

            puts("Pretending to mine");
            for (unsigned i = pool_size-1; i > 1 ; i--) {
                /* TODO: Mine Transaction */
                if (pool_ptr[i].id_self != 0) {
                    pool_ptr[i].id_self = 0;
                    printf("[Miner Thread %d] Mining transaction in slot: %d\n", id, i);
                    break;
                }
            }

            sem_post(g_sem_pool_mutex);
            sem_post(g_sem_pool_empty);
        } else if (errno == EAGAIN) {
            sleep(1);
        } else {
            perror("sem_trywait failed");
            break;
        }

    }

    printf("[Miner Thread %d] Shutdown set to 0, exiting thread.\n", id);
    // mc_threads[id-1] = NULL; // ?
    pthread_exit(NULL);
}

void mc_cleanup() {
    shutdown = 1; // Ensure threads exit
    //
    #ifdef DEBUG
    puts("[Miner controller] Sending");
    #endif

    for (i = 0; i < created_threads; i++) {
        sem_post(g_sem_pool_full); // Unblock sem_wait
        sem_post(g_sem_pool_empty);
    }

    #ifdef DEBUG
    puts("[Miner controller] Joining threads");
    #endif

    // Clean up threads
    for (unsigned i = 0; i < created_threads; i++) {
        if (mc_threads[i]) {
            pthread_join(mc_threads[i], NULL);
        }
    }

    #ifdef DEBUG
    puts("[Miner controller] Freeing resources");
    #endif

    free(mc_threads);
    free(id_array);
    c_cleanup(); // Cleanup semaphores/shared memory
}

void mc_handle_sigint(int sig) {
    if (sig != SIGINT) return;
    shutdown = 1; // Signal threads to exit
    sigint_received = 1; // Signal main thread to exit
}

/* Function Declarations */
void c_mc_main(unsigned int miners_max) {

    mc_threads  = malloc(sizeof(pthread_t) * miners_max );
    id_array    = malloc(sizeof(int) * miners_max );

    /* Create Miner Threads */
    for (i = 0; i < miners_max; i++) {
        id_array[i] = i+1;
        if (pthread_create(&mc_threads[i], NULL, miner_thread, &id_array[i]) != 0) {
            perror("pthread_create failed");
            break; // Stop creating threads on failure
        }
        created_threads++;
    }

    // Set up signal handler
    struct sigaction sa;
    sa.sa_handler = mc_handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    #ifdef DEBUG
    puts("[Miner controller] Entering main loop");
    #endif

    // Wait for shutdown signal
    while (!sigint_received) {
        pause();
    }

    #ifdef DEBUG
    puts("[Miner controller] Starting cleanup");
    #endif

    // Perform final cleanup
    mc_cleanup();

    #ifdef DEBUG
    puts("[Miner controller] Exit complete");
    #endif
    exit(0);
}
