#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <platform.h>
#include <stdint.h>
#include <stdbool.h>

PONY_EXTERN_C_BEGIN

typedef uint64_t (*hash_fn)(void* data);

typedef bool (*cmp_fn)(void* a, void* b);

typedef bool (*pred_fn)(void* a, void* arg);

typedef bool (*resolve_fn)(void* a, void* b, void* arg);

typedef void* (*map_fn)(void* a, void* arg);

typedef bool (*apply_fn)(void* a, void* arg);

typedef void* (*dup_fn)(void* data);

typedef void (*free_fn)(void* data);

#define DECLARE_TYPE(name, elem) \
  typedef struct name##_t name##_t; \
  typedef uint64_t (*name##_hash_fn)(elem* a); \
  typedef bool (*name##_cmp_fn)(elem* a, elem* b); \
  typedef bool (*name##_pred_fn)(elem* a, void* arg); \
  typedef bool (*name##_resolve_fn)(elem* a, elem* b, void* arg); \
  typedef elem* (*name##_map_fn)(elem* a, void* arg); \
  typedef bool (*name##_apply_fn)(elem* a, void* arg); \
  typedef elem* (*name##_dup_fn)(elem* a); \
  typedef void (*name##_free_fn)(elem* a); \

PONY_EXTERN_C_END

#endif
