#ifndef ASYNC_VECTOR_H
#define ASYNC_VECTOR_H

#define VECTOR_INIT_CAPACITY 4

typedef struct vector {
    void **items;
    int capacity;
    int total;
} vector;

void vector_init(vector *);

int vector_total(vector *);

int vector_add(vector *, void *);

void *vector_get(vector *, int);

void vector_delete(vector *, int);

void vector_free(vector *);

#endif //ASYNC_VECTOR_H
