#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#define N 10
int numero=0;
//Só mutex? só podem entrar um de cada
// Shared memory como é?
// Realease está bem?
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
void *worker(void* i)
	{
	int w_id=*((int *)i);
	pthread_mutex_lock(&mutex);
	printf("Numero partilhado, eu sou miner %d %d\n",w_id,numero);
	numero +=1;
	pthread_mutex_unlock(&mutex);
	pthread_exit(NULL);
	}


int main() {
	int i, worker_id[N];
  	pthread_t my_thread[N];
  //initialize processing_list array 
  	FILE *file;
    char buffer[255]; // Buffer to store each line
    
    file = fopen("Configure.config", "r");
    if (file == NULL) {
        printf("File open error\n");
        return 1;
    }
    
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        printf("%s", buffer);
    }
    
    fclose(file);
  // create N threads
  for (i = 0; i < N; i++) {
    worker_id[i]=i+1;
    pthread_create(&my_thread[i], NULL, worker, &worker_id[i]);
  }
  for(i=0; i< N; i++){
    pthread_join(my_thread[i], NULL);
  }

  printf("Done\n");
  exit(0);
    return 0;
}
