#include <stdio.h>
#include <stdlib.h>

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

static void *squared_free(void *arg, size_t argsz __attribute__((unused)),
                          size_t *retsz __attribute__((unused))) {
    int n = *(int *) arg;
    int *ret = malloc(sizeof(int));
    *ret = n * n;
    free(arg);
    return ret;
}

static void *div2_free(void *arg, size_t argsz __attribute__((unused)),
                       size_t *retsz __attribute__((unused))) {
    int n = *(int *) arg;
    int *ret = malloc(sizeof(int));
    *ret = n / 2;
    free(arg);
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
    thread_pool_init(&pool, 2);

    future_t tab[5];

    int n = 2;
    async(&pool, &tab[0],
          (callable_t) {.function = squared, .arg = &n, .argsz = sizeof(int)});
    map(&pool, &tab[1], &tab[0], squared_free);
    map(&pool, &tab[2], &tab[1], div2_free);
    map(&pool, &tab[3], &tab[2], squared_free);
    map(&pool, &tab[4], &tab[3], squared_free);
    int *m = await(&tab[4]);

    mu_assert("expected 64 * 64", *m == 64 * 64);
    free(m);

    thread_pool_destroy(&pool);
    return 0;
}

static char *test_map_simple2() {
    thread_pool_init(&pool, 1);

    future_t tab[5];

    int n = 2;
    async(&pool, &tab[0],
          (callable_t) {.function = squared, .arg = &n, .argsz = sizeof(int)});
    map(&pool, &tab[1], &tab[0], squared_free);
    map(&pool, &tab[2], &tab[1], div2_free);
    map(&pool, &tab[3], &tab[2], squared_free);
    int k = 0;
    for (int i = 0; i < 12341241; i++) {
        k = k * 12 + 1312;
        k %= 31209312;
    }
    map(&pool, &tab[4], &tab[3], squared_free);
    int *m = await(&tab[4]);

    mu_assert("nie", k != *m);
    mu_assert("expected 64 * 64", *m == 64 * 64);
    free(m);

    thread_pool_destroy(&pool);
    return 0;
}


static char *test_map_simple3() {
    thread_pool_init(&pool, 1);

    future_t tab[5];

    int n = 2;
    async(&pool, &tab[0],
          (callable_t) {.function = squared, .arg = &n, .argsz = sizeof(int)});
    map(&pool, &tab[1], &tab[0], squared_free);
    map(&pool, &tab[2], &tab[1], div2_free);
    map(&pool, &tab[3], &tab[2], squared_free);
    map(&pool, &tab[4], &tab[3], squared_free);
    int *mm = await(&tab[4]);
    mu_assert("expected 64 * 64", *mm == 64 * 64);
    free(mm);

    future_t tab2[5];
    int n2 = 3;
    async(&pool, &tab2[0],
          (callable_t) {.function = squared, .arg = &n2, .argsz = sizeof(int)});
    map(&pool, &tab2[1], &tab2[0], squared_free);
    map(&pool, &tab2[2], &tab2[1], div2_free);
    map(&pool, &tab2[3], &tab2[2], squared_free);
    map(&pool, &tab2[4], &tab2[3], squared_free);
    int *m = await(&tab2[4]);

    mu_assert("expected 2560000", *m == 2560000);
    free(m);

    thread_pool_destroy(&pool);
    return 0;
}

static char *all_tests() {
    mu_run_test(test_await_simple);
    mu_run_test(test_map_simple);
    mu_run_test(test_map_simple2);
    mu_run_test(test_map_simple3);
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