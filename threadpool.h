#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stdio.h>
#include <stddef.h>
#include <sys/types.h>

/**
 * Implementation of ThreadPool and Runnable with blocking queue.
 */

#define err(str) fprintf(stderr, str)

/* ========================== STRUCTURES ============================ */
typedef struct runnable {
    void (*function)(void *, size_t);

    void *arg;
    size_t argsz;
} runnable_t;

typedef struct bin_sem {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    size_t v;
} bsem;

typedef struct job {
    struct job *prev; /*Pointer to the previous job*/
    runnable_t job;
} job;

typedef struct jobqueue {
    pthread_mutex_t r_w_mutex; /*Mutex for read/write on queue*/
    job *front;                /*Pointer to the front job in the queue*/
    job *rear;                 /*Pointer to the rear job in the queue*/
    bsem *has_jobs;
    size_t len;                /*Number of the jobs in the queue*/
} jobqueue;

typedef struct thread {
    pthread_t pthread;                 /*Pointer to the actual thread*/
    struct thread_pool *thread_pool_p; /*Ensures access to the thread pool*/
} thread;

typedef struct thread_pool {
    int id;
    size_t keepAlive;
    thread **threads;              /*Pointer to the threads in thread pool*/
    volatile size_t num_threads_alive;
    volatile size_t num_threads_working;
    jobqueue *jobqueue;
    pthread_mutex_t thcount_lock;
    pthread_cond_t threads_idle;
} thread_pool_t;

/* ================================================================== */

int thread_pool_init(thread_pool_t *pool, size_t pool_size);

void thread_pool_destroy(thread_pool_t *pool);

int defer(thread_pool_t *pool, runnable_t runnable);

#endif
