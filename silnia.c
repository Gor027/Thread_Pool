#include <stdlib.h>
#include "threadpool.h"
#include "future.h"

#define NO_THREADS 3

typedef struct myJob {
    long long start;
    long long number;
    thread_pool_t *pool;
} myJob;

void *inside_callable(void *arg, size_t argsz __attribute__ ((unused)), size_t *resultSz __attribute__ ((unused))) {
    myJob *job = arg;
    long long *n = malloc(sizeof(long long));
    *n = job->start;

    for (long long i = job->start + NO_THREADS; i <= job->number; i += NO_THREADS) {
        *n *= i;
    }

    return n;
}

void *start_callable(void *arg, size_t argsz __attribute__ ((unused)), size_t *resultSz __attribute__ ((unused))) {
    myJob **jobs = arg;
    long long **results = malloc(NO_THREADS * sizeof(long long *));
    future_t *futures[NO_THREADS];

    for (size_t i = 0; i < NO_THREADS; i++) {
        myJob *job = jobs[i];
        future_t *new_future = malloc(sizeof(future_t));
        async(job->pool, new_future, (callable_t) {.function = inside_callable, .arg = job, .argsz = sizeof(myJob)});
        futures[i] = new_future;
    }

    for (size_t j = 0; j < NO_THREADS; ++j) {
        results[j] = (long long *) await(futures[j]);
    }

    for (size_t i = 0; i < NO_THREADS; ++i) {
        free(futures[i]);
    }

    return results;
}

void *result_callable(void *arg, size_t argsz __attribute__ ((unused)), size_t *resultSz __attribute__ ((unused))) {
    long long **results = arg;
    long long *ans = malloc(sizeof(long long));

    *ans = 1;
    for (size_t i = 0; i < NO_THREADS; ++i) {
        *ans *= *results[i];
    }

    for (size_t i = 0; i < NO_THREADS; ++i) {
        free(results[i]);
    }

    free(results);

    return ans;
}

int main() {

    thread_pool_t pool;
    thread_pool_init(&pool, NO_THREADS);

    long long n;

    /* Case 0, 1 and 2 are hard coded as base cases. */
    while (scanf("%lld", &n) != EOF) {
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

        for (long long i = 1; i <= NO_THREADS; i++) {
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

        long long *ans = await(&result_future);

        printf("%lld\n", *ans);

        free(ans);
        for (size_t i = 0; i < NO_THREADS; ++i) {
            free(jobs[i]);
        }
    }

    thread_pool_destroy(&pool);

    return 0;
}