#include <stdio.h>
#include <stdlib.h>
#include "threadpool.h"
#include <unistd.h>
#include <pthread.h>

#define NO_THREADS 4

double floor(double value) {
    return (double) (int) value;
}

typedef struct my_job {
    int row;
    int val;
    int sleep_time;
} my_job;

static int *row_sum = NULL;
static int count = 0;
static pthread_mutex_t guard;
static pthread_cond_t cond;

void runnable_function(void *arg, size_t argsz __attribute__((unused))) {
    my_job *job = (my_job *) arg;

    /* Sleeps until a specified amount of time */
    usleep(job->sleep_time * 1000);

    pthread_mutex_lock(&guard);
    row_sum[job->row] += job->val;
    count++;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&guard);
}

int main() {
    pthread_mutex_init(&guard, 0);
    pthread_cond_init(&cond, 0);

    int rows, columns;
    scanf("%d", &rows);
    scanf("%d", &columns);

    thread_pool_t pool;
    thread_pool_init(&pool, NO_THREADS);

    row_sum = calloc(rows, sizeof(int));

    my_job **jobArray = malloc(rows * columns * sizeof(my_job *));
    runnable_t **runArray = malloc(rows * columns * sizeof(runnable_t));
    for (int i = 0; i < rows * columns; ++i) {
        /* Creating Job */
        my_job *new_my_job = malloc(sizeof(my_job));

        new_my_job->row = floor((double) i / (double) columns);

        scanf("%d", &new_my_job->val);
        scanf("%d", &new_my_job->sleep_time);

        jobArray[i] = new_my_job;

        /* Creating Runnable */
        runnable_t *new_runnable = malloc(sizeof(runnable_t));

        new_runnable->function = runnable_function;
        new_runnable->arg = new_my_job;
        new_runnable->argsz = sizeof(my_job);

        runArray[i] = new_runnable;

        /* Submit task to the thread pool */
        defer(&pool, *new_runnable);
    }

    pthread_mutex_lock(&guard);
    while (count != rows * columns) {
        pthread_cond_wait(&cond, &guard);
    }
    pthread_mutex_unlock(&guard);

    thread_pool_destroy(&pool);

    for (int i = 0; i < rows; ++i) {
        printf("%d\n", row_sum[i]);
    }

    for (int i = 0; i < rows * columns; ++i) {
        free(jobArray[i]);
        free(runArray[i]);
    }

    free(jobArray);
    free(runArray);
    free(row_sum);

    return 0;
}