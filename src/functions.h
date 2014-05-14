#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <stdint.h>
#include <stdbool.h>

typedef uint64_t (*hash_fn)(const void* data);

typedef bool (*cmp_fn)(const void* a, const void* b);

typedef bool (*pred_fn)(void* arg, void* iter);

typedef void* (*map_fn)(void* arg, void* iter);

typedef void* (*dup_fn)(const void* data);

typedef void (*free_fn)(void* data);

#endif
