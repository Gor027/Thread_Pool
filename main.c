#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

#include "threadpool.h"
#include "test/minunit.h"

void runnable_function(void *arg, size_t argsz) {
    long sum = 0;

    for (int i = 0; i < 1000000000; ++i) {
        sum += i;
    }

    printf("%lu", sum);
}

int main() {
    thread_pool_t pool;
    thread_pool_init(&pool, 4);

    for (int i = 0; i < 4; ++i) {
        defer(&pool, (runnable_t) {.function = runnable_function, .arg = NULL, .argsz = 0});
    }

    thread_pool_destroy(&pool);

    return 0;
}
