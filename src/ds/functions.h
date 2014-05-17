#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <stdint.h>
#include <stdbool.h>

typedef uint64_t (*hash_fn)(void* data);

typedef bool (*cmp_fn)(void* a, void* b);

typedef bool (*pred_fn)(void* arg, void* a);

typedef void* (*map_fn)(void* arg, void* a);

typedef void* (*dup_fn)(void* data);

typedef void (*free_fn)(void* data);

#define DECLARE_TYPE(name, elem) \
  typedef struct name##_t name##_t; \
  typedef uint64_t (*name##_hash_fn)(elem* a); \
  typedef bool (*name##_cmp_fn)(elem* a, elem* b); \
  typedef bool (*name##_pred_fn)(void* arg, elem* a); \
  typedef elem* (*name##_map_fn)(void* arg, elem* a); \
  typedef elem* (*name##_dup_fn)(elem* a); \
  typedef void (*name##_free_fn)(elem* a); \

#endif
