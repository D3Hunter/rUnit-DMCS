////////////////////////////////////////////////////////////////////////////////
//               Copyright(c) 2013 SDU, All right reserved
// Filename        :    threadPool.cpp
// Projectname     :    RenderingUnit
// Author          :    Jujj
// Email           :    
// Date            :    2013-07-08
// Version         :    v 1.0
// Description     :    
////////////////////////////////////////////////////////////////////////////////
#include "threadPool.h"
#include "common.h"

static CThread_pool *pool = NULL;


void pool_init (int max_thread_num) 
{ 
     pool = (CThread_pool *) malloc (sizeof (CThread_pool)); 
     pthread_mutex_init (&(pool->queue_lock), NULL); 
     pthread_cond_init (&(pool->queue_ready), NULL); 
     pool->queue_head = NULL; 
     pool->max_thread_num = max_thread_num; 
     pool->cur_queue_size = 0; 
     pool->shutdown=0; 
     pool->threadid=(pthread_t *) malloc (max_thread_num * sizeof (pthread_t)); 
     int i = 0; 
     for (i = 0;i<max_thread_num;i++) 
     { 
         pthread_create(&(pool->threadid[i]),NULL,thread_routine,NULL); 
     } 
} 

int pool_add_worker (void *(*process) (void *arg),void *arg) 
{
     CThread_worker *newworker=(CThread_worker *) malloc (sizeof (CThread_worker)); 
     newworker->process = process; 
     newworker->arg = arg; 
     newworker->next = NULL;

     pthread_mutex_lock(&(pool->queue_lock)); 
     CThread_worker *member=pool->queue_head; 
     if (member!=NULL) 
     { 
         while(member->next!=NULL) 
             member=member->next; 
         member->next = newworker; 
     } 
     else 
     { 
         pool->queue_head=newworker; 
     } 
     assert (pool->queue_head != NULL); 

     pool->cur_queue_size++; 
     pthread_mutex_unlock (&(pool->queue_lock));
     pthread_cond_signal (&(pool->queue_ready)); 
     return 0; 
} 

int pool_destroy () 
{ 
    if (pool->shutdown) 
        return -1;
     pool->shutdown = 1; 

     pthread_cond_broadcast (&(pool->queue_ready)); 

    int i; 
    for (i=0;i<pool->max_thread_num; i++)
    {
         pthread_join (pool->threadid[i], NULL); 
    }
    free (pool->threadid); 
    CThread_worker *head = NULL; 
    while (pool->queue_head != NULL) 
    { 
         head = pool->queue_head; 
         pool->queue_head = pool->queue_head->next; 
         free (head); 
     } 
         
     pthread_mutex_destroy(&(pool->queue_lock)); 
     pthread_cond_destroy(&(pool->queue_ready)); 
     
     free (pool); 
     pool=NULL; 
     return 0; 
} 

void wait_pool_complete(void)
{
    while(0 != pool->cur_queue_size) 
    {
        printf("---------Waiting-------\n");
        mySleep(1);
    }
}

void * thread_routine (void *arg) 
{ 
     //printf ("starting thread 0x%x\n", pthread_self ()); 
     //pthread_detach(pthread_self());
     while (1) 
     { 
         pthread_mutex_lock (&(pool->queue_lock)); 

         while (pool->cur_queue_size == 0 && !pool->shutdown) 
         { 
             //printf ("thread 0x%x is waiting\n", pthread_self ()); 
             pthread_cond_wait (&(pool->queue_ready), &(pool->queue_lock)); 
         } 
	
         if (pool->shutdown) 
         { 
             pthread_mutex_unlock (&(pool->queue_lock)); 
             //printf ("thread 0x%x will exit\n", pthread_self ()); 
             pthread_exit (NULL); 
         } 
	
         //printf ("thread 0x%x is starting to work\n", pthread_self ()); 
	
         assert (pool->cur_queue_size != 0); 
         assert (pool->queue_head != NULL); 
         
         pool->cur_queue_size--; 
         CThread_worker *worker = pool->queue_head; 
         pool->queue_head = worker->next; 
         pthread_mutex_unlock (&(pool->queue_lock)); 
         (*(worker->process)) (worker->arg); 
         free (worker); 
         worker = NULL; 
     } 	
     pthread_exit (NULL); 
} 

