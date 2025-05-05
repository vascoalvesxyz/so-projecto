//Vasco Alves 2022228207 Joao Neto 2023234004
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>

#include "controller.h"   // has semaphore macros
#include "transaction.h"  // has struct Transaction definition

Transaction transcation_generate() {
    /* id_self, id_sender, id_reciever, timestamp, reward, value */
    return (Transaction) { 0, 1, 2, 3, 4, 5 };
}

int main(int argc, char *argv[]) {
	// Open semaphores
	if(argc!=3) {
        printf("Wrong number of parameters\n");
        exit(0);
    }

    sem_t *sem_empty = sem_open(SEM_POOL_EMPTY, 0);
    sem_t *sem_full = sem_open(SEM_POOL_FULL, 0);
    sem_t *sem_mutex = sem_open(SEM_POOL_MUTEX, 0);

    if (sem_empty == SEM_FAILED || sem_full == SEM_FAILED || sem_mutex == SEM_FAILED) {
        printf("Failed to open semaphores. Is controller running?\n");
        exit(EXIT_FAILURE);
    }
    //ADICIONAR LOGICA DE CONTINUO TRANSACTION
    

    int reward = atoi(argv[1]);
    int time_ms = atoi(argv[2]);

    if(reward >=1 && reward <=3 && time_ms <=3000 && time_ms >=200){
        printf("The parameters are correct.\n");
    }

    sem_close(sem_empty);
    sem_close(sem_full);
    sem_close(sem_mutex);

    return 0;
}
