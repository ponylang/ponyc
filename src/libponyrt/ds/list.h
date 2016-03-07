#ifndef ds_list_h
#define ds_list_h

#include "fun.h"
#include <stdint.h>
#include <stdbool.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN

typedef struct list_t list_t;

list_t* ponyint_list_pop(list_t* list, void** data);

list_t* ponyint_list_push(list_t* list, void* data);

list_t* ponyint_list_append(list_t* list, void* data);

list_t* ponyint_list_next(list_t* list);

list_t* ponyint_list_index(list_t* list, ssize_t index);

void* ponyint_list_data(list_t* list);

void* ponyint_list_find(list_t* list, cmp_fn f, void* data);

ssize_t ponyint_list_findindex(list_t* list, cmp_fn f, void* data);

bool ponyint_list_subset(list_t* a, list_t* b, cmp_fn f);

bool ponyint_list_equals(list_t* a, list_t* b, cmp_fn f);

list_t* ponyint_list_map(list_t* list, map_fn f, void* arg);

list_t* ponyint_list_reverse(list_t* list);

size_t ponyint_list_length(list_t* list);

void ponyint_list_free(list_t* list, free_fn f);

#define DECLARE_LIST(name, name_t, elem) \
  typedef struct name_t name_t; \
  typedef bool (*name##_cmp_fn)(elem* a, elem* b); \
  typedef elem* (*name##_map_fn)(elem* a, void* arg); \
  typedef void (*name##_free_fn)(elem* a); \
  name_t* name##_pop(name_t* list, elem** data); \
  name_t* name##_push(name_t* list, elem* data); \
  name_t* name##_append(name_t* list, elem* data); \
  name_t* name##_next(name_t* list); \
  name_t* name##_index(name_t* list, ssize_t index); \
  elem* name##_data(name_t* list); \
  elem* name##_find(name_t* list, elem* data); \
  ssize_t name##_findindex(name_t* list, elem* data); \
  bool name##_subset(name_t* a, name_t* b); \
  bool name##_equals(name_t* a, name_t* b); \
  name_t* name##_map(name_t* list, name##_map_fn f, void* arg); \
  name_t* name##_reverse(name_t* list); \
  size_t name##_length(name_t* list); \
  void name##_free(name_t* list); \

#define DEFINE_LIST(name, name_t, elem, cmpf, freef) \
  struct name_t {}; \
  \
  name_t* name##_pop(name_t* list, elem** data) \
  { \
    return (name_t*)ponyint_list_pop((list_t*)list, (void**)data); \
  } \
  name_t* name##_push(name_t* list, elem* data) \
  { \
    return (name_t*)ponyint_list_push((list_t*)list, (void*)data); \
  } \
  name_t* name##_append(name_t* list, elem* data) \
  { \
    return (name_t*)ponyint_list_append((list_t*)list, (void*)data); \
  } \
  name_t* name##_next(name_t* list) \
  { \
    return (name_t*)ponyint_list_next((list_t*)list); \
  } \
  name_t* name##_index(name_t* list, ssize_t index) \
  { \
    return (name_t*)ponyint_list_index((list_t*)list, index); \
  } \
  elem* name##_data(name_t* list) \
  { \
    return (elem*)ponyint_list_data((list_t*)list); \
  } \
  elem* name##_find(name_t* list, elem* data) \
  { \
    name##_cmp_fn cmp = cmpf; \
    return (elem*)ponyint_list_find((list_t*)list, (cmp_fn)cmp, (void*)data); \
  } \
  ssize_t name##_findindex(name_t* list, elem* data) \
  { \
    name##_cmp_fn cmp = cmpf; \
    return ponyint_list_findindex((list_t*)list, (cmp_fn)cmp, (void*)data); \
  } \
  bool name##_subset(name_t* a, name_t* b) \
  { \
    name##_cmp_fn cmp = cmpf; \
    return ponyint_list_subset((list_t*)a, (list_t*)b, (cmp_fn)cmp); \
  } \
  bool name##_equals(name_t* a, name_t* b) \
  { \
    name##_cmp_fn cmp = cmpf; \
    return ponyint_list_equals((list_t*)a, (list_t*)b, (cmp_fn)cmp); \
  } \
  name_t* name##_map(name_t* list, name##_map_fn f, void* arg) \
  { \
    return (name_t*)ponyint_list_map((list_t*)list, (map_fn)f, arg); \
  } \
  name_t* name##_reverse(name_t* list) \
  { \
    return (name_t*)ponyint_list_reverse((list_t*)list); \
  } \
  size_t name##_length(name_t* list) \
  { \
    return ponyint_list_length((list_t*)list); \
  } \
  void name##_free(name_t* list) \
  { \
    name##_free_fn free = freef; \
    ponyint_list_free((list_t*)list, (free_fn)free); \
  } \

PONY_EXTERN_C_END

#endif
