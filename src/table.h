#ifndef TABLE_H
#define TABLE_H

#include "functions.h"

typedef struct table_t table_t;

table_t* table_create(int size, hash_fn hash, cmp_fn cmp, dup_fn dup, free_fn fr);

bool table_merge(table_t* dst, const table_t* src);

void* table_find(const table_t* table, void* entry);

void* table_insert(table_t* table, void* entry, bool* present);

void table_free(table_t* table);

#endif
