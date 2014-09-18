#ifndef ds_list_h
#define ds_list_h

#include "fun.h"
#include <stdint.h>
#include <stdbool.h>

#include <platform.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct list_t list_t;

list_t* list_pop(list_t* list, void** data);

list_t* list_push(list_t* list, void* data);

list_t* list_append(list_t* list, void* data);

list_t* list_next(list_t* list);

list_t* list_index(list_t* list, ssize_t index);

void* list_data(list_t* list);

void* list_find(list_t* list, cmp_fn f, void* data);

ssize_t list_findindex(list_t* list, cmp_fn f, void* data);

bool list_subset(list_t* a, list_t* b, cmp_fn f);

bool list_equals(list_t* a, list_t* b, cmp_fn f);

list_t* list_map(list_t* list, map_fn f, void* arg);

list_t* list_reverse(list_t* list);

size_t list_length(list_t* list);

void list_free(list_t* list, free_fn f);

#define DECLARE_LIST(name, elem) \
  typedef struct name##_t name##_t; \
  typedef bool (*name##_cmp_fn)(elem* a, elem* b); \
  typedef elem* (*name##_map_fn)(elem* a, void* arg); \
  typedef void (*name##_free_fn)(elem* a); \
  name##_t* name##_pop(name##_t* list, elem** data); \
  name##_t* name##_push(name##_t* list, elem* data); \
  name##_t* name##_append(name##_t* list, elem* data); \
  name##_t* name##_next(name##_t* list); \
  name##_t* name##_index(name##_t* list, ssize_t index); \
  elem* name##_data(name##_t* list); \
  elem* name##_find(name##_t* list, elem* data); \
  ssize_t name##_findindex(name##_t* list, elem* data); \
  bool name##_subset(name##_t* a, name##_t* b); \
  bool name##_equals(name##_t* a, name##_t* b); \
  name##_t* name##_map(name##_t* list, name##_map_fn f, void* arg); \
  name##_t* name##_reverse(name##_t* list); \
  size_t name##_length(name##_t* list); \
  void name##_free(name##_t* list); \

#define DEFINE_LIST(name, elem, cmpf, freef) \
  struct name##_t {}; \
  \
  name##_t* name##_pop(name##_t* list, elem** data) \
  { \
    return (name##_t*)list_pop((list_t*)list, (void**)data); \
  } \
  name##_t* name##_push(name##_t* list, elem* data) \
  { \
    return (name##_t*)list_push((list_t*)list, data); \
  } \
  name##_t* name##_append(name##_t* list, elem* data) \
  { \
    return (name##_t*)list_append((list_t*)list, data); \
  } \
  name##_t* name##_next(name##_t* list) \
  { \
    return (name##_t*)list_next((list_t*)list); \
  } \
  name##_t* name##_index(name##_t* list, ssize_t index) \
  { \
    return (name##_t*)list_index((list_t*)list, index); \
  } \
  elem* name##_data(name##_t* list) \
  { \
    return (elem*)list_data((list_t*)list); \
  } \
  elem* name##_find(name##_t* list, elem* data) \
  { \
    name##_cmp_fn cmp = cmpf; \
    return (elem*)list_find((list_t*)list, (cmp_fn)cmp, data); \
  } \
  ssize_t name##_findindex(name##_t* list, elem* data) \
  { \
    name##_cmp_fn cmp = cmpf; \
    return list_findindex((list_t*)list, (cmp_fn)cmp, data); \
  } \
  bool name##_subset(name##_t* a, name##_t* b) \
  { \
    name##_cmp_fn cmp = cmpf; \
    return list_subset((list_t*)a, (list_t*)b, (cmp_fn)cmp); \
  } \
  bool name##_equals(name##_t* a, name##_t* b) \
  { \
    name##_cmp_fn cmp = cmpf; \
    return list_equals((list_t*)a, (list_t*)b, (cmp_fn)cmp); \
  } \
  name##_t* name##_map(name##_t* list, name##_map_fn f, void* arg) \
  { \
    return (name##_t*)list_map((list_t*)list, (map_fn)f, arg); \
  } \
  name##_t* name##_reverse(name##_t* list) \
  { \
    return (name##_t*)list_reverse((list_t*)list); \
  } \
  size_t name##_length(name##_t* list) \
  { \
    return list_length((list_t*)list); \
  } \
  void name##_free(name##_t* list) \
  { \
    name##_free_fn free = freef; \
    list_free((list_t*)list, (free_fn)free); \
  } \

#ifdef __cplusplus
}
#endif

#endif
