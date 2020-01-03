#include "threadpool.h"
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include "vector.h"

/* ========================== FUNCTION PROTOTYPES ============================ */
static void bsem_init(bsem *bsem_p, size_t v);

static void bsem_reset(bsem *bsem_p);

static void bsem_wait(bsem *bsem_p);

static void bsem_notify(bsem *bsem_p);

static void bsem_notifyAll(bsem *bsem_p);

static int thread_init(thread_pool_t *pool, thread **thread_p);

static void thread_destroy(thread *thread_p);

static void *thread_do(thread *thread_p);

static int jobqueue_init(jobqueue *jobqueue_p);

static void jobqueue_clear(jobqueue *jobqueue_p);

static void jobqueue_push(jobqueue *jobqueue_p, job *job_p);

static job *jobqueue_pull(jobqueue *jobqueue_p);

static void jobqueue_destroy(jobqueue *jobqueue_p);

/* =========================================================================== */

static volatile size_t keepAlive;
static vector v;

static __attribute__ ((constructor)) void vec_initializer() {
    vector_init(&v);
}

static __attribute__ ((destructor)) void vec_destroyer() {
    vector_free(&v);
}

/* ========================== THREADPOOL ============================ */
/**
 * Destroys all pools.
 */
static void handler() {
    /* TODO */
}

/**
 * Function for handling SIGINT behaviour.
 */
static void set_sig_handler(void) {
    struct sigaction action = {0};

    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = handler;

    if (sigaction(SIGINT, &action, NULL) == -1) {
        err("set_sig_handler(): SIGINT signal handling failed.\n");
    }
}


int thread_pool_init(thread_pool_t *pool, size_t num_threads) {
    if (pool == NULL) {
        err("thread_pool_init(): thread_pool is a null pointer.\n");
        return -1;
    }

    keepAlive = 1;

    pool->num_threads_alive = 0;
    pool->num_threads_working = 0;

    /* ThreadPool should have a job queue */
    pool->jobqueue = malloc(sizeof(struct jobqueue));

    if (pool->jobqueue == NULL) {
        err("jobqueue_init(): Malloc failed for job queue.\n");
        return -1;
    }

    if (jobqueue_init(pool->jobqueue) == -1) {
        err("thread_pool_init(): Initialising job queue failed.\n");
        return -1;
    }

    /* Creating the threads in the thread pool */
    pool->threads = malloc(num_threads * sizeof(struct thread *));
    if (pool->threads == NULL) {
        err("thread_pool_init(): Malloc failed for creating threads\n");
        jobqueue_destroy(pool->jobqueue);
        free(pool);
        return -1;
    }

    /* Initialise mutex and condition in thread pool */
    if (pthread_mutex_init(&pool->thcount_lock, 0)) {
        err("thread_pool_init(): mutex initialisation failed.\n");
        jobqueue_destroy(pool->jobqueue);
        free(pool->threads);
        free(pool);
        return -1;
    }

    if (pthread_cond_init(&pool->threads_idle, 0) != 0) {
        err("thread_pool_init(): condition initialisation failed.\n");
        return -1;
    }

    set_sig_handler();

    for (size_t i = 0; i < num_threads; ++i) {
        thread_init(pool, &pool->threads[i]);
    }

    /* Waiting to all threads to be initialised */
    while (pool->num_threads_alive != num_threads) {
        usleep(100 * 1000);
    }


    return 0;
}

void thread_pool_destroy(thread_pool_t *pool) {
    /* Return if thread pool is NULL */
    if (pool == NULL) {
        err("thread_pool_destroy(): destroy is called on null pointer.\n");
        return;
    }

    volatile size_t totalThreads = pool->num_threads_alive;

    /* Each threads infinite loop should be ended */
    keepAlive = 0;

    /* Kill threads */
    while (pool->num_threads_alive) {
        bsem_notifyAll(pool->jobqueue->has_jobs);
        /* Sleep on second to let threads to end then continue */
        usleep(100 * 1000);
    }

    /* Destroying job queue of the thread pool */
    jobqueue_destroy(pool->jobqueue);

    /* Free allocated memories */
    for (size_t i = 0; i < totalThreads; ++i) {
        thread_destroy(pool->threads[i]);
    }

    pthread_mutex_destroy(&pool->thcount_lock);
    pthread_cond_destroy(&pool->threads_idle);

    free(pool->threads);
    free(pool->jobqueue);
}

int defer(thread_pool_t *pool, runnable_t runnable) {
    if (pool == NULL) {
        err("defer(): defer is called on null pointer.\n");
        return -1;
    }

    if (keepAlive == 0) {
        err("defer(): After thread_pool_destroy defer is called.\n");
        return -1;
    }

    job *job_p;

    job_p = malloc(sizeof(struct job));

    if (job_p == NULL) {
        err("defer(): Malloc failed for new submitted task.\n");
        return -1;
    }

    job_p->job = runnable;

    jobqueue_push(pool->jobqueue, job_p);

    return 0;
}

/* ================================================================== */

/* ============================ THREAD ============================== */

static int thread_init(thread_pool_t *pool, thread **thread_p) {
    *thread_p = malloc(sizeof(struct thread));

    if (*thread_p == NULL) {
        err("thread_init(): Malloc failed for thread initialisation.\n");
        return -1;
    }

    (*thread_p)->thread_pool_p = pool;

    pthread_create(&(*thread_p)->pthread, NULL, (void *) thread_do, (*thread_p));

    /* Threads in the thread pool are disconnected and
     * it should not be possible to wait until they end.
     * In short, threads are UnJoinable.
     */
    pthread_detach((*thread_p)->pthread);

    return 0;
}

/* Just frees the allocated memory for a thread struct */
static void thread_destroy(thread *thread_p) {
    free(thread_p);
}

/**
 * Function blocks SIGINT signal for the current thread.
 */
static void mask_sig(void) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);

    pthread_sigmask(SIG_BLOCK, &mask, NULL);
}

static void *thread_do(thread *thread_p) {
    /* SIGINT should be blocked for thread_pool threads. */
    mask_sig();

    thread_pool_t *pool = thread_p->thread_pool_p;

    pthread_mutex_lock(&pool->thcount_lock);
    pool->num_threads_alive += 1;
    pthread_mutex_unlock(&pool->thcount_lock);

    /* keepAlive will be set to 0 while destroying the thread pool of the thread */
    while (keepAlive || pool->jobqueue->len != 0) {
        bsem_wait(pool->jobqueue->has_jobs);

        if (keepAlive || pool->jobqueue->len != 0) {
            pthread_mutex_lock(&pool->thcount_lock);
            pool->num_threads_working += 1;
            pthread_mutex_unlock(&pool->thcount_lock);

            /* Get job from job queue and process */
            void (*func)(void *, size_t);
            void *arg;
            size_t argsz;

            job *job_p = jobqueue_pull(pool->jobqueue);

            if (job_p != NULL) {
                func = job_p->job.function;
                arg = job_p->job.arg;
                argsz = job_p->job.argsz;

                func(arg, argsz);
                free(job_p);
            }

            pthread_mutex_lock(&pool->thcount_lock);
            pool->num_threads_working -= 1;

            if (!pool->num_threads_working)
                pthread_cond_signal(&pool->threads_idle);

            pthread_mutex_unlock(&pool->thcount_lock);
        }
    }

    pthread_mutex_lock(&pool->thcount_lock);
    pool->num_threads_alive -= 1;
    pthread_mutex_unlock(&pool->thcount_lock);

    /* NULL on SUCCESS */
    return NULL;
}

/* ================================================================== */

/* ============================ JOB QUEUE =========================== */

static int jobqueue_init(jobqueue *jobqueue_p) {
    jobqueue_p->len = 0;
    jobqueue_p->front = NULL;
    jobqueue_p->rear = NULL;

    jobqueue_p->has_jobs = malloc(sizeof(struct bin_sem));

    if (jobqueue_p->has_jobs == NULL) {
        err("jobqueue_init(): Malloc failed for has_jobs.\n");
        return -1;
    }

    if (pthread_mutex_init(&(jobqueue_p->r_w_mutex), 0) != 0) {
        err("jobqueue_init(): mutex initialisation failed.\n");
        return -1;
    }

    bsem_init(jobqueue_p->has_jobs, 0);

    return 0;
}

static void jobqueue_clear(jobqueue *jobqueue_p) {
    while (jobqueue_p->len) {
        free(jobqueue_pull(jobqueue_p));
    }

    jobqueue_p->front = NULL;
    jobqueue_p->rear = NULL;
    bsem_reset(jobqueue_p->has_jobs);
    jobqueue_p->len = 0;
}

static void jobqueue_push(jobqueue *jobqueue_p, job *job_p) {
    pthread_mutex_lock(&(jobqueue_p->r_w_mutex));

    job_p->prev = NULL;

    switch (jobqueue_p->len) {
        case 0:
            jobqueue_p->front = job_p;
            jobqueue_p->rear = job_p;
            break;
        default:
            jobqueue_p->rear->prev = job_p;
            jobqueue_p->rear = job_p;
    }

    jobqueue_p->len += 1;

    bsem_notifyAll(jobqueue_p->has_jobs);
    pthread_mutex_unlock(&jobqueue_p->r_w_mutex);
}

static job *jobqueue_pull(jobqueue *jobqueue_p) {
    pthread_mutex_lock(&jobqueue_p->r_w_mutex);

    job *job_p = jobqueue_p->front;

    switch (jobqueue_p->len) {
        case 0:
            /* No jobs in the queue */
            break;
        case 1:
            jobqueue_p->front = NULL;
            jobqueue_p->rear = NULL;
            jobqueue_p->len = 0;
            break;
        default:
            jobqueue_p->front = job_p->prev;
            jobqueue_p->len -= 1;
            bsem_notify(jobqueue_p->has_jobs);
    }

    pthread_mutex_unlock(&jobqueue_p->r_w_mutex);

    return job_p;
}

static void jobqueue_destroy(jobqueue *jobqueue_p) {
    jobqueue_clear(jobqueue_p);
    free(jobqueue_p->has_jobs);
}

/* ================================================================== */


/* ======================== SYNCHRONIZATION ON JOBS ========================= */

static void bsem_init(bsem *bsem_p, size_t v) {
    if (v > 1) {
        err("bsem_init(): Bin Semaphore can be initialised with value 0 or 1.\n");
        exit(1);
    }

    pthread_mutex_init(&bsem_p->mutex, 0);
    pthread_cond_init(&bsem_p->cond, 0);
    bsem_p->v = v;
}

static void bsem_reset(bsem *bsem_p) {
    bsem_init(bsem_p, 0);
}

static void bsem_wait(bsem *bsem_p) {
    pthread_mutex_lock(&bsem_p->mutex);

    while (bsem_p->v != 1) {
        pthread_cond_wait(&bsem_p->cond, &bsem_p->mutex);
    }

    bsem_p->v = 0;
    pthread_mutex_unlock(&bsem_p->mutex);
}

/* Notify one thread waiting to get a job */
static void bsem_notify(bsem *bsem_p) {
    pthread_mutex_lock(&bsem_p->mutex);
    bsem_p->v = 1;
    pthread_cond_signal(&bsem_p->cond);
    pthread_mutex_unlock(&bsem_p->mutex);
}

/* Notify all threads */
static void bsem_notifyAll(bsem *bsem_p) {
    pthread_mutex_lock(&bsem_p->mutex);
    bsem_p->v = 1;
    pthread_cond_broadcast(&bsem_p->cond);
    pthread_mutex_unlock(&bsem_p->mutex);
}

/* ========================================================================== */

