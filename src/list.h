#ifndef LIST_H
#define LIST_H

#include "functions.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct list_t list_t;

list_t* list_push(list_t* list, const void* data);

list_t* list_append(list_t* list, const void* data);

list_t* list_next(list_t* list);

list_t* list_index(list_t* list, int index);

void* list_data(list_t* list);

void* list_find(list_t* list, cmp_fn f, const void* data);

bool list_superset(list_t* list, list_t* sublist, cmp_fn f);

bool list_equals(list_t* a, list_t* b, cmp_fn f);

bool list_test(list_t* list, pred_fn f, void* arg);

list_t* list_map(list_t* list, map_fn f, void* arg);

uint64_t list_hash(list_t* list, hash_fn f, uint64_t seed);

int list_length(list_t* list);

void list_free(list_t* list, free_fn f);

#endif
