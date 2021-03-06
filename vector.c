#include <stdio.h>
#include <stdlib.h>

#include "vector.h"

void vector_init(vector *v) {
    v->capacity = VECTOR_INIT_CAPACITY;
    v->total = 0;
    v->items = malloc(sizeof(void *) * v->capacity);
}

int vector_total(vector *v) {
    return v->total;
}

static void vector_resize(vector *v, int capacity) {
    void **items = realloc(v->items, sizeof(void *) * capacity);

    if (items) {
        v->items = items;
        v->capacity = capacity;
    }
}

int vector_add(vector *v, void *item) {
    if (v->capacity == v->total)
        vector_resize(v, v->capacity * 2);

    v->items[v->total++] = item;

    return v->total - 1;
}

void *vector_get(vector *v, int index) {
    if (index >= 0 && index < v->total)
        return v->items[index];

    return NULL;
}

void vector_delete(vector *v, int index) {
    if (index < 0 || index >= v->total)
        return;

    v->items[index] = NULL;
}

void vector_free(vector *v) {
    free(v->items);
}