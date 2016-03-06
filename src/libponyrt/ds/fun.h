#ifndef ds_fun_h
#define ds_fun_h

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN

typedef size_t (*hash_fn)(void* arg);

typedef bool (*cmp_fn)(void* a, void* b);

typedef void* (*map_fn)(void* a, void* arg);

typedef void* (*alloc_fn)(size_t size);

typedef void (*free_fn)(void* data);

typedef void (*free_size_fn)(size_t size, void* data);

uint64_t ponyint_hash_block(const void* p, size_t len);

uint64_t ponyint_hash_str(const char* str);

size_t ponyint_hash_ptr(const void* p);

uint64_t ponyint_hash_int64(uint64_t key);

uint32_t ponyint_hash_int32(uint32_t key);

size_t ponyint_hash_size(size_t key);

size_t ponyint_next_pow2(size_t i);

PONY_EXTERN_C_END

#endif
