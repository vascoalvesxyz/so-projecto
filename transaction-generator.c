//Vasco Alves 2022228207 Joao Neto 2023234004
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#include "controller.h"   // has semaphore macros
#include "transaction.h"  // has struct Transaction definition

/* Transaction Generator (TxGen). A process executed by the user that produces transactions at 
 * specified intervals and writes them to a Transaction Pool (located in shared memory). Several 
 * Transaction Generator processes may be active at the same time. The TxGen adds a transaction 
 * to the Transaction Pool by traversing it sequentially, placing it in the first available spot, and 
 * making the aging field zero. */

int   g_pool_fd;
void  *g_pool_ptr;
size_t g_pool_size;
sem_t *g_sem_empty;
sem_t *g_sem_full; 
sem_t *g_sem_mutex;

int t_init() {
    /* init shared memory */
    int fd = shm_open(SHMEM_PATH_POOL, O_RDWR, 0666);
    if (fd < 0) {
        puts("Failed to open shared memory. Is controller running?");
        return fd;
    } 

    ShmemInfo info;
    if (read(fd, &info, sizeof(Transaction)) < 0) {
            return -1;
    }

    size_t size = (size_t) info.id_self;
    printf("size = %ld\n", size);

    void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        puts("Failed to allocate mapped memory.");
        close(fd);
        return -1;
    }

    /* initializar semaforos */
    sem_t *sem_empty = sem_open(SEM_POOL_EMPTY, 0);
    sem_t *sem_full = sem_open(SEM_POOL_FULL, 0);
    sem_t *sem_mutex = sem_open(SEM_POOL_MUTEX, 0);
    if (sem_empty == SEM_FAILED || sem_full == SEM_FAILED || sem_mutex == SEM_FAILED) {
        puts("Failed to open semaphores. Is controller running?");
        return -1;
    }

    /* Initializado correctament */
    g_pool_fd = fd;
    g_pool_ptr = ptr;
    g_pool_size = size;

    g_sem_full = sem_full;
    g_sem_empty = sem_empty;
    g_sem_mutex = sem_mutex;
    return 0;
}

void t_cleanup() {
    /* Shared Memory */
    munmap(g_pool_ptr, g_pool_size); 
    close(g_pool_fd);

    /* Semaphores */
    sem_close(g_sem_empty);
    sem_close(g_sem_full);
    sem_close(g_sem_mutex);
}


Transaction transcation_generate() {
    /* id_self, id_sender, id_reciever, timestamp, reward, value */
    return (Transaction) { 0, 1, 2, 3, 4, 5 };
}

int main(int argc, char *argv[]) {

	/* Check arguments */
	if(argc != 3) {
        puts("Wrong number of parameters\n");
        exit(EXIT_FAILURE);
    }

    int reward = atoi(argv[1]);
    int time_ms = atoi(argv[2]);
    if(reward < 1 || reward  > 3 || time_ms > 3000 || time_ms < 200){
        puts("TxGen [reward 1-3] [time_ms 200-3000]");
        exit(EXIT_FAILURE);
    }

    /* Initialize */
    if (t_init() < 0) {
        exit(EXIT_FAILURE);
    }

    /* Clean exit */
    t_cleanup();
    return 0;
}
