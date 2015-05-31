#ifndef POOL_H
#define	POOL_H

#include <sys/sem.h>
#include <sys/ipc.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <bits/pthreadtypes.h> 
#include <assert.h> 

typedef struct worker 
{ 
    void *(*process) (void *arg); 
    void *arg;
    struct worker *next; 

} CThread_worker; 

typedef struct 
{ 
    pthread_mutex_t queue_lock; 
    pthread_cond_t queue_ready; 

    CThread_worker *queue_head; 

    int shutdown; 
    pthread_t *threadid; 
    int max_thread_num; 
    int cur_queue_size; 

} CThread_pool; 

void pool_init (int max_thread_num);

int pool_add_worker (void *(*process) (void *arg),void *arg);

int pool_destroy ();
void wait_pool_complete(void);
void *thread_routine (void *arg);

#endif
