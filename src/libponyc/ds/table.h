#ifndef TABLE_H
#define TABLE_H

#include "functions.h"
#include <stddef.h>

typedef struct table_t table_t;

table_t* table_create(size_t size);

bool table_merge(table_t* dst, table_t* src, hash_fn hash, cmp_fn cmp,
  dup_fn dup, pred_fn pred, void* pred_arg);

void* table_find(table_t* table, void* entry, hash_fn hash, cmp_fn cmp);

void* table_insert(table_t* table, void* entry, bool* present,
  hash_fn hash, cmp_fn cmp, dup_fn dup);

void table_free(table_t* table, free_fn fr);

#define DECLARE_TABLE(name, elem) \
  DECLARE_TYPE(name, elem); \
  name##_t* name##_create(int size); \
  bool name##_merge(name##_t* dst, name##_t* src, name##_pred_fn pred, \
    void* pred_arg); \
  elem* name##_find(name##_t* table, elem* entry); \
  elem* name##_insert(name##_t* table, elem* entry, bool* present); \
  void name##_free(name##_t* table); \

#define DEFINE_TABLE(name, elem, hash, cmp, dup, fr) \
  DECLARE_TYPE(name, elem); \
  struct name##_t {}; \
  \
  name##_t* name##_create(int size) \
  { \
    return (name##_t*)table_create(size); \
  } \
  bool name##_merge(name##_t* dst, name##_t* src, name##_pred_fn pred, \
    void* pred_arg) \
  { \
    name##_hash_fn hashf = hash; \
    name##_cmp_fn cmpf = cmp; \
    name##_dup_fn dupf = dup; \
    return table_merge((table_t*)dst, (table_t*)src, \
      (hash_fn)hashf, (cmp_fn)cmpf, (dup_fn)dupf, (pred_fn)pred, pred_arg); \
  } \
  elem* name##_find(name##_t* table, elem* entry) \
  { \
    name##_hash_fn hashf = hash; \
    name##_cmp_fn cmpf = cmp; \
    return (elem*)table_find((table_t*)table, (void*)entry, \
      (hash_fn)hashf, (cmp_fn)cmpf); \
  } \
  elem* name##_insert(name##_t* table, elem* entry, bool* present) \
  { \
    name##_hash_fn hashf = hash; \
    name##_cmp_fn cmpf = cmp; \
    name##_dup_fn dupf = dup; \
    return (elem*)table_insert((table_t*)table, (void*)entry, present, \
      (hash_fn)hashf, (cmp_fn)cmpf, (dup_fn)dupf); \
  } \
  void name##_free(name##_t* table) \
  { \
    name##_free_fn frf = fr; \
    table_free((table_t*)table, (free_fn)frf); \
  } \

#endif
