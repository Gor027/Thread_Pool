#include <pthread.h>
#include "future.h"
#include <stdlib.h>
#include <unistd.h>

typedef void *(*function_t)(void *, size_t, size_t *);

int future_init(future_t *future);

void future_set(future_t *future, void *result, size_t resultSz);

void *future_get(future_t *future);

void future_destroy(future_t *future);

typedef struct wrap {
    callable_t callable;
    future_t *future;
    runnable_t *runnable;
} wrap_t;

typedef struct map_wrap {
    future_t *future_from;
    function_t func;
    future_t *new_future;
    runnable_t *runnable;
} map_wrap_t;

/**
 * The function is used as a runnable function in map function
 * to create a new value for a future from another future.
 * @param arg   - argument of the runnable;
 * @param argsz - size of the argument of runnable.
 */
void map_runnable(void *arg, size_t argsz __attribute__ ((unused))) {
    map_wrap_t *wrapper = arg;

    future_t *fut = wrapper->future_from;

    /* Thread may stop here to wait until future_value is done. */
    void *futResult = future_get(fut);
    /* When future_value is done its size is also set. */
    size_t futResultSz = fut->resultSz;

    function_t func = wrapper->func;

    size_t resultSz = 0;
    void *result = func(futResult, futResultSz, &resultSz);
    future_set(wrapper->new_future, result, resultSz);

    /* Destroys the mutex and condition in the future but not the result of it. */
    future_destroy(fut);
    free(wrapper->runnable);
    free(wrapper);
}

/**
 * The function is used as a runnable function for async function
 * to wrap callable into runnable function and submit runnable task
 * instead of callable task to the thread_pool.
 * @param arg   - argument of the runnable;
 * @param argsz - size of the argument of the runnable.
 */
void runnable_function(void *arg, size_t argsz __attribute__ ((unused))) {
    wrap_t *wrapper = arg;
    size_t result_size = 0;

    void *(*callFunc)(void *, size_t, size_t *) = wrapper->callable.function;
    void *result = callFunc(wrapper->callable.arg, wrapper->callable.argsz, &result_size);

    future_set(wrapper->future, result, result_size);

    free(wrapper->runnable);
    free(wrapper);
}

/**
 * Function creates a new runnable out of a wrapper struct.
 * @param wrapper - pointer to a wrapper struct.
 * @return - returns a pointer to a new runnable, otherwise null.
 */
runnable_t *callable_to_runnable(wrap_t *wrapper) {

    runnable_t *new_runnable = malloc(sizeof(runnable_t));

    if (new_runnable == NULL) {
        err("callable_to_runnable(): malloc failed for runnable.\n");
        return NULL;
    }

    new_runnable->function = runnable_function;
    new_runnable->arg = wrapper;
    new_runnable->argsz = sizeof(wrap_t);

    return new_runnable;
}

/**
 * Submits new task/callable to thread_pool jobqueue.
 * Result is an initialised future with the result and result size.
 * @param pool - pointer on the thread_pool
 * @param future - pointer on the future which carries the result of the callable task.
 * @param callable - callable task to be submitted to thread_pool.
 * @return 0 on success, otherwise -1 if some failures happened.
 */
int async(thread_pool_t *pool, future_t *future, callable_t callable) {
    if (future_init(future) == -1) {
        err("async(): future_init() failed as future is NULL.\n");
        return -1;
    }

    wrap_t *wrapper = malloc(sizeof(wrap_t));

    if (wrapper == NULL) {
        err("async(): malloc failed for creating wrapper.\n");
        return -1;
    }

    wrapper->callable = callable;
    wrapper->future = future;
    wrapper->runnable = NULL;

    runnable_t *my_runnable = callable_to_runnable(wrapper);

    if (my_runnable == NULL) {
        err("async(): malloc failed for creating new runnable.\n");
        return -1;
    }

    wrapper->runnable = my_runnable;

    if (defer(pool, *my_runnable) != 0) {
        err("async(): Submitting new callable task failed.\n");
        free(wrapper);
        free(my_runnable);
        return -1;
    }

    return 0;
}

/**
 * Function uses 'from' future and 'function' to map a new future.
 * Multiple maps on the same future is an undefined behaviour, as
 * after mapping future 'from' is destroyed.
 * @param pool     - pointer on thread_pool
 * @param future   - pointer on future to be mapped
 * @param from     - pointer on base future to map another future
 * @param function - function pointer to map the new future
 * @return 0 on success, otherwise -1 if some failures happened.
 */
int map(thread_pool_t *pool, future_t *future, future_t *from,
        void *(*function)(void *, size_t, size_t *)) {

    if (future_init(future) == -1) {
        err("map(): future_init() failed as future is NULL.\n");
        return -1;
    }

    map_wrap_t *wrapper = malloc(sizeof(map_wrap_t));
    if (wrapper == NULL) {
        err("map(): malloc failed for creating map_wrapper.\n");
        return -1;
    }

    wrapper->func = function;
    wrapper->future_from = from;
    wrapper->new_future = future;
    wrapper->runnable = NULL;

    runnable_t *my_runnable = malloc(sizeof(runnable_t));
    if (my_runnable == NULL) {
        err("map(): malloc failed for creating runnable.\n");
        return -1;
    }

    my_runnable->function = map_runnable;
    my_runnable->arg = wrapper;
    my_runnable->argsz = sizeof(map_wrap_t);

    wrapper->runnable = my_runnable;

    if (defer(pool, *my_runnable) != 0) {
        err("map(): Submitting new task failed.\n");
        free(wrapper);
        free(my_runnable);
        return -1;
    }

    return 0;
}

/**
 * Blocking function. Calling thread waits until future result
 * will be ready to take. After await, future is destroyed, so
 * multiple awaits on the same future is undefined behaviour.
 * @param future - pointer to the future
 * @return pointer to the result of future, which may also be NULL.
 */
void *await(future_t *future) {
    void *res = future_get(future);
    future_destroy(future);

    return res;
}

/**
 * Initializes future parameters.
 * @param future - pointer to a future.
 * @return - returns 0 on success, otherwise -1;
 */
int future_init(future_t *future) {
    if (future == NULL) {
        err("future_init(): future is a null pointer.\n");
        return -1;
    }

    future->result = NULL;
    future->resultSz = 0;
    future->done = false;

    if (pthread_mutex_init(&future->mutex, NULL) != 0) {
        err("future_init(): mutex initialisation failed.\n");
        return -1;
    }

    if (pthread_cond_init(&future->cond, NULL) != 0) {
        err("future_init(): condition initialisation failed.\n");
        return -1;
    }

    return 0;
}

/**
 * Sets the result and result size of the future.
 * @param future   - pointer on the future.
 * @param result   - pointer on the result.
 * @param resultSz - size of the result of the future.
 */
void future_set(future_t *future, void *result, size_t resultSz) {
    pthread_mutex_lock(&future->mutex);

    future->result = result;
    future->resultSz = resultSz;
    future->done = true;

    /* Broadcast as other futures may wait to be created out of this future. */
    pthread_cond_broadcast(&future->cond);

    pthread_mutex_unlock(&future->mutex);
}

/**
 * Returns the future value.
 * Function is a blocking function as future may be not ready to get its result.
 * @param future - pointer on the future.
 * @return returns future value as it gets ready.
 */
void *future_get(future_t *future) {
    pthread_mutex_lock(&future->mutex);

    while (!future->done) {
        pthread_cond_wait(&future->cond, &future->mutex);
    }

    pthread_mutex_unlock(&future->mutex);

    return future->result;
}

/**
 * Destroys the future by destroying its mutex and condition.
 * Note that result cannot be free-d as it is the responsibility of the user.
 * @param future - pointer on the future.
 */
void future_destroy(future_t *future) {
    pthread_mutex_destroy(&future->mutex);
    pthread_cond_destroy(&future->cond);
}