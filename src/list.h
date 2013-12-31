#ifndef LIST_H
#define LIST_H

#include <stdint.h>
#include <stdbool.h>

typedef struct list_t list_t;

typedef uint64_t (*list_hash_fn)(void* data, uint64_t seed);

typedef bool (*list_cmp_fn)(void* a, void* b);

typedef bool (*list_pred_fn)(void* arg, list_t* list);

typedef void* (*list_map_fn)(void* arg, list_t* list);

typedef void (*list_free_fn)(void* data);

list_t* list_push(list_t* list, const void* data);

list_t* list_append(list_t* list, const void* data);

list_t* list_next(list_t* list);

list_t* list_index(list_t* list, int index);

void* list_data(list_t* list);

bool list_has(list_t* list, list_cmp_fn f, void* data);

bool list_superset(list_t* list, list_t* sublist, list_cmp_fn f);

bool list_equals(list_t* a, list_t* b, list_cmp_fn f);

bool list_test(list_t* list, list_pred_fn f, void* arg);

list_t* list_map(list_t* list, list_map_fn f, void* arg);

uint64_t list_hash(list_t* list, list_hash_fn f, uint64_t seed);

int list_length(list_t* list);

void list_free(list_t* list, list_free_fn f);

#endif
