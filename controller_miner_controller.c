//Vasco Alves 2022228207 Joao Neto 2023234004
#include <strings.h>
#include <signal.h>
#include "controller.h"

typedef struct MinerThreadArgs {
    pthread_mutex_t *mutex;
    int id;
} MinerThreadArgs;
//GET TRANSACTIONS
//PROCESS THE BLOCKS (usar função fornecida)
//SEND TO VALIDATOR

/* "private" Variables */
static unsigned int id=0;

/* Function Declarations */
void c_mc_main(unsigned int miners_max) {

    MinerThreadArgs args[miners_max];
    pthread_t mc_miner_thread_arr[miners_max];

    unsigned int i = 0, created_threads = 0;
    pthread_mutex_t miner_mutex = PTHREAD_MUTEX_INITIALIZER;

    void handle_sigint() {
        /* wait for miner threads to be done */
        for(i = 0; i < created_threads; i++){
            pthread_join(mc_miner_thread_arr[i], NULL);
        }

        pthread_mutex_destroy(&miner_mutex);
        c_logputs("Miner Controller: Exited successfully!\n");
        c_cleanup();
        exit(EXIT_SUCCESS);
    }

    signal(SIGINT, handle_sigint);

    void* miner_thread(void* args_ptr) {
        MinerThreadArgs args = *( (MinerThreadArgs*) args_ptr );
        pthread_mutex_lock(args.mutex);
        id +=1;
        pthread_mutex_unlock(args.mutex);
        c_logprintf("miner %d: id=%d\n",args.id,id);
        pthread_exit(NULL);
    }

    for (i = 0; i < miners_max; i++, created_threads++) {
        args[i] = (MinerThreadArgs) {&miner_mutex, i+1};
        pthread_create(&mc_miner_thread_arr[i], NULL, miner_thread, (void*) &args[i]);
    }

    handle_sigint(0);
}
