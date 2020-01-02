#include <stdlib.h>
#include "threadpool.h"
#include "future.h"

#define NO_THREADS 3

typedef struct myJob {
    size_t start;
    size_t number;
    thread_pool_t *pool;
} myJob;

void *inside_callable(void *arg, size_t argsz __attribute__ ((unused)), size_t *resultSz __attribute__ ((unused))) {
    myJob *job = arg;
    size_t *n = malloc(sizeof(size_t));
    *n = job->start;

    for (size_t i = job->start + NO_THREADS; i <= job->number; i += NO_THREADS) {
        *n *= i;
    }

    return n;
}

void *start_callable(void *arg, size_t argsz __attribute__ ((unused)), size_t *resultSz __attribute__ ((unused))) {
    myJob **jobs = arg;
    size_t **results = malloc(NO_THREADS * sizeof(size_t *));
    future_t *futures[NO_THREADS];

    for (size_t i = 0; i < NO_THREADS; i++) {
        myJob *job = jobs[i];
        future_t *new_future = malloc(sizeof(future_t));
        async(job->pool, new_future, (callable_t) {.function = inside_callable, .arg = job, .argsz = sizeof(myJob)});
        futures[i] = new_future;
    }

    for (int j = 0; j < NO_THREADS; ++j) {
        results[j] = (size_t *) await(futures[j]);
    }

    for (int i = 0; i < NO_THREADS; ++i) {
        free(futures[i]);
    }

    return results;
}

void *result_callable(void *arg, size_t argsz __attribute__ ((unused)), size_t *resultSz __attribute__ ((unused))) {
    size_t **results = arg;
    size_t *ans = malloc(sizeof(size_t));

    *ans = 1;
    for (int i = 0; i < NO_THREADS; ++i) {
        *ans *= *results[i];
    }

    for (int i = 0; i < NO_THREADS; ++i) {
        free(results[i]);
    }

    free(results);

    return ans;
}

int main() {

    thread_pool_t pool;
    thread_pool_init(&pool, NO_THREADS);

    size_t n;

    while (scanf("%zd", &n) != EOF) {
        if (n == 0 || n == 1) {
            printf("1\n");
            continue;
        }

        if (n == 2) {
            printf("2\n");
            continue;
        }

        myJob *jobs[NO_THREADS];
        future_t new_future;

        for (size_t i = 1; i <= NO_THREADS; i++) {
            myJob *new_job = malloc(sizeof(myJob));
            new_job->start = i;
            new_job->number = n;
            new_job->pool = &pool;

            jobs[i - 1] = new_job;
        }

        async(&pool, &new_future,
              (callable_t) {.function = start_callable, .arg = jobs, .argsz = NO_THREADS * sizeof(myJob *)});

        future_t result_future;

        map(&pool, &result_future, &new_future, result_callable);

        size_t *ans = await(&result_future);

        printf("%zd\n", *ans);

        free(ans);
        for (int i = 0; i < NO_THREADS; ++i) {
            free(jobs[i]);
        }
    }

    thread_pool_destroy(&pool);

    return 0;
}