//Vasco Alves 2022228207 Joao Neto 2023234004
#include <stdlib.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include <openssl/sha.h>

#include "deichain.h"  // has struct Transaction definition

#ifdef DEBUG
int _macro_buf;
#endif /* ifdef DEBUG */

#define SEM_POOL_EMPTY          "/dei_pool_empty"
#define SEM_POOL_FULL           "/dei_pool_full"
#define SEM_POOL_MUTEX          "/dei_pool_mutex"
#define SHMEM_PATH_POOL         "/dei_transaction_pool"

/* Transaction Generator (TxGen). A process executed by the user that produces transactions at 
 * specified intervals and writes them to a Transaction Pool (located in shared memory). Several 
 * Transaction Generator processes may be active at the same time. The TxGen adds a transaction 
 * to the Transaction Pool by traversing it sequentially, placing it in the first available spot, and 
 * making the aging field zero. */

int   g_pool_fd;
TransactionPool *g_pool_ptr;
size_t g_pool_size_read;
sem_t *g_sem_empty;
sem_t *g_sem_full; 
sem_t *g_sem_mutex;

int transaction_n = 0;
pid_t my_pid;

/* Helper par dormir em millisegundos */
static inline void sleep_ms(int milliseconds) {
  struct timespec ts;
  ts.tv_sec = milliseconds / 1000;
  ts.tv_nsec = (milliseconds % 1000) * 1000000;
  while (nanosleep(&ts, &ts) == -1) { }
}

int t_init() {
  /* init shared memory */
  int fd = shm_open(SHMEM_PATH_POOL, O_RDWR, 0666);
  if (fd < 0) {
    puts("Failed to open shared memory. Is controller running?");
    return fd;
  } 

  TransactionPool info;
  if (read(fd, &info, sizeof(TransactionPool)) < 0) {
    return -1;
  }

  size_t size = (size_t) info.tx.reward;
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
  g_pool_size_read = size;

  g_sem_full = sem_full;
  g_sem_empty = sem_empty;
  g_sem_mutex = sem_mutex;

  my_pid = getpid();
  return 0;
}

void t_cleanup() {
  /* Shared Memory */
  munmap(g_pool_ptr, g_pool_size_read); 
  close(g_pool_fd);

  /* Semaphores */
  sem_close(g_sem_empty);
  sem_close(g_sem_full);
  sem_close(g_sem_mutex);
}

void handle_sigint() {
  t_cleanup();
  exit(0);
}


Transaction transaction_generate(int reward) {
  /* tx_id, reward, value, timestamp */
  Transaction new_transaction;
  memset(&new_transaction, 0, sizeof(Transaction));
  new_transaction.reward = reward;
  new_transaction.value = rand();
  new_transaction.timestamp = time(NULL);

  uint32_t input = my_pid+transaction_n;
  SHA256( (void*) &input, sizeof(uint32_t), &new_transaction.tx_id[0]);
  return new_transaction;
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

  signal(SIGINT,  handle_sigint);

  size_t pool_size = ( g_pool_size_read/sizeof(TransactionPool) ) - 1; // subtract first transaction
  TransactionPool *pool_ptr = g_pool_ptr + 1; // ignore first transaction
  //

  while (1) {
    puts("[TxGen] Waiting...");

    //sem_wait(g_sem_empty);
#ifdef DEBUG
    PRINT_SEM("EMPTY", g_sem_empty);
#endif

    // if(sem_trywait(g_sem_empty) !=0)
    //   continue;

    sem_wait(g_sem_empty);
    for (unsigned i = 0; i < pool_size ; i++) {
      /* Find empty transaction */
      if (pool_ptr[i].empty == 0) {
        TransactionPool new_transaction = (TransactionPool) {transaction_generate(reward), 0, 1};

        // sem_wait(g_sem_mutex);
        pool_ptr[i] = new_transaction;
        // sem_post(g_sem_mutex);
        printf("[TxGen] Inserting transaction in slot: %d\n", i);
        transaction_n++; // increment transaction counter
        break;
      }
      if(i== pool_size-1)
        printf("End of pool\n");
    }

#ifdef DEBUG
    PRINT_SEM("FULL", g_sem_full);
#endif

    sem_post(g_sem_full);
    sleep_ms(time_ms);

  }

  /* Clean exit */
  t_cleanup();
  return 0;
}
