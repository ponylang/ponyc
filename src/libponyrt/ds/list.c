#include "list.h"
#include "../gc/serialise.h"
#include "../mem/pool.h"
#include "ponyassert.h"
#include <stdlib.h>

list_t* ponyint_list_pop(list_t* list, void** data)
{
  list_t* l = list;
  list = l->next;
  *data = l->data;
  POOL_FREE(list_t, l);

  return list;
}

list_t* ponyint_list_push(list_t* list, void* data)
{
  list_t* l = (list_t*)POOL_ALLOC(list_t);
  l->data = data;
  l->next = list;

  return l;
}

list_t* ponyint_list_append(list_t* list, void* data)
{
  list_t* l = (list_t*)POOL_ALLOC(list_t);
  l->data = data;
  l->next = NULL;

  if(list == NULL)
    return l;

  list_t* p = list;

  while(p->next != NULL)
    p = p->next;

  p->next = l;
  return list;
}

list_t* ponyint_list_next(list_t* list)
{
  return list->next;
}

list_t* ponyint_list_index(list_t* list, ssize_t index)
{
  if(index < 0)
    index = (ssize_t)ponyint_list_length(list) + index;

  for(int i = 0; (list != NULL) && (i < index); i++)
    list = list->next;

  return list;
}

void* ponyint_list_data(list_t* list)
{
  return list->data;
}

void* ponyint_list_find(list_t* list, cmp_fn f, void* data)
{
  while(list != NULL)
  {
    if(f((void*)data, list->data))
      return list->data;

    list = list->next;
  }

  return NULL;
}

ssize_t ponyint_list_findindex(list_t* list, cmp_fn f, void* data)
{
  size_t index = 0;

  while(list != NULL)
  {
    if(f((void*)data, list->data))
      return index;

    list = list->next;
    index++;
  }

  return -1;
}

bool ponyint_list_subset(list_t* a, list_t* b, cmp_fn f)
{
  while(a != NULL)
  {
    if(ponyint_list_find(b, f, a->data) == NULL)
      return false;

    a = a->next;
  }

  return true;
}

bool ponyint_list_equals(list_t* a, list_t* b, cmp_fn f)
{
  while(a != NULL)
  {
    if((b == NULL) || !f(a->data, b->data))
      return false;

    a = a->next;
    b = b->next;
  }

  return b == NULL;
}

list_t* ponyint_list_map(list_t* list, map_fn f, void* arg)
{
  list_t* to = NULL;

  while(list != NULL)
  {
    void* result = f(list->data, (void*)arg);

    if(result != NULL)
      to = ponyint_list_push(to, result);

    list = list->next;
  }

  return ponyint_list_reverse(to);
}

list_t* ponyint_list_reverse(list_t* list)
{
  list_t* to = NULL;

  while(list)
  {
    list_t* next = list->next;
    list->next = to;
    to = list;
    list = next;
  }

  return to;
}

size_t ponyint_list_length(list_t* list)
{
  size_t len = 0;

  while(list != NULL)
  {
    len++;
    list = list->next;
  }

  return len;
}

void ponyint_list_free(list_t* list, free_fn f)
{
  list_t* next;

  while(list != NULL)
  {
    next = list->next;

    if(f != NULL)
      f(list->data);

    POOL_FREE(list_t, list);
    list = next;
  }
}

void ponyint_list_serialise_trace(pony_ctx_t* ctx, void* object,
  pony_type_t* list_type, pony_type_t* elem_type)
{
  list_t* list = (list_t*)object;

  if(list->data != NULL)
    pony_traceknown(ctx, list->data, elem_type, PONY_TRACE_MUTABLE);

  if(list->next != NULL)
    pony_traceknown(ctx, list->next, list_type, PONY_TRACE_MUTABLE);
}

void ponyint_list_serialise(pony_ctx_t* ctx, void* object, void* buf,
  size_t offset)
{
  list_t* list = (list_t*)object;
  list_t* dst = (list_t*)((uintptr_t)buf + offset);

  dst->data = (void*)pony_serialise_offset(ctx, list->data);
  dst->next = (list_t*)pony_serialise_offset(ctx, list->next);
}

void ponyint_list_deserialise(pony_ctx_t* ctx, void* object,
  pony_type_t* list_type, pony_type_t* elem_type)
{
  list_t* list = (list_t*)object;

  list->data = pony_deserialise_offset(ctx, elem_type, (uintptr_t)list->data);
  list->next = (list_t*)pony_deserialise_offset(ctx, list_type,
    (uintptr_t)list->next);
}
