#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "future.h"
#include "test/minunit.h"

int tests_run = 0;
static thread_pool_t pool;
static future_t future;

static void *squared(void *arg, size_t argsz __attribute__((unused)),
                     size_t *retsz __attribute__((unused))) {
    int n = *(int *) arg;
    int *ret = malloc(sizeof(int));
    *ret = n * n;
    return ret;
}

static char *test_await_simple() {
    thread_pool_init(&pool, 2);

    int n = 16;
    async(&pool, &future,
          (callable_t) {.function = squared, .arg = &n, .argsz = sizeof(int)});
    int *m = await(&future);

    mu_assert("expected 256", *m == 256);
    free(m);

    thread_pool_destroy(&pool);
    return 0;
}

static char *test_map_simple() {
    assert(!thread_pool_init(&pool, 1));

    int n = 2;

    future_t future1, future2, future3;
    assert(!async(&pool, &future,
                  (callable_t) {.function = squared, .arg = &n, .argsz = sizeof(int)}));
    assert(!map(&pool, &future1, &future, squared));
    sleep(2);
    assert(!map(&pool, &future2, &future1, squared));
    sleep(2);
    assert(!map(&pool, &future3, &future2, squared));
    sleep(2);
    int *m = await(&future3);

    mu_assert("expected 256 * 256", *m == 256 * 256);
    free(m);
    free(future1.result);
    free(future2.result);

    thread_pool_destroy(&pool);

    return 0;
}

static char *all_tests() {
    mu_run_test(test_await_simple);
    mu_run_test(test_map_simple);
    return 0;
}

int main() {
    char *result = all_tests();
    if (result != 0) {
        printf(__FILE__ " %s\n", result);
    } else {
        printf(__FILE__ " ALL TESTS PASSED\n");
    }
    printf(__FILE__ " Tests run: %d\n", tests_run);

    return result != 0;
}
