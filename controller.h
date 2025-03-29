
#ifndef _CONTROLLER_H_
#define _CONTROLLER_H_

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>

#define FPATH_LOG "DEIChain_log.txt"

/* log related variables will be accessed by subprocesses */
extern char _buf[512];
extern FILE *g_logfile_fptr;
extern pthread_mutex_t g_logfile_mutex; 

void c_logputs(const char* string);
#define c_logprintf(...)\
        sprintf(_buf, __VA_ARGS__);\
        c_logputs(_buf)

void c_cleanup();

int  c_ctrl_init();               // Initialize global variables;
int  c_ctrl_import_config();     // import configuration
void c_ctrl_cleanup();           // destroy global variables
void c_ctrl_handle_sigint();

void c_val_main();
void c_stat_main();
void c_mc_main(unsigned int miners_max);

#endif // !_CONTROLLER_H_
