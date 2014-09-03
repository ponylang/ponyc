#include "list.h"
#include <stdlib.h>
#include <assert.h>

struct list_t
{
  void* data;
  struct list_t* next;
};

list_t* list_push(list_t* list, const void* data)
{
  list_t* l = (list_t*)malloc(sizeof(list_t));
  l->data = (void*)data;
  l->next = list;

  return l;
}

list_t* list_append(list_t* list, const void* data)
{
  list_t* l = (list_t*)malloc(sizeof(list_t));
  l->data = (void*)data;
  l->next = NULL;

  if(list == NULL)
    return l;

  list_t* p = list;

  while(p->next != NULL)
    p = p->next;

  p->next = l;
  return list;
}

list_t* list_next(list_t* list)
{
  return list->next;
}

list_t* list_index(list_t* list, ssize_t index)
{
  if(index < 0)
    index = list_length(list) + index;

  for(int i = 0; (list != NULL) && (i < index); i++)
    list = list->next;

  return list;
}

void* list_data(list_t* list)
{
  return list->data;
}

void* list_find(list_t* list, cmp_fn f, const void* data)
{
  while(list != NULL)
  {
    if(f((void*)data, list->data))
      return list->data;

    list = list->next;
  }

  return NULL;
}

int list_findindex(list_t* list, cmp_fn f, const void* data)
{
  int index = 0;

  while(list != NULL)
  {
    if(f((void*)data, list->data))
      return index;

    list = list->next;
    index++;
  }

  return -1;
}

bool list_subset(list_t* a, list_t* b, cmp_fn f)
{
  while(a != NULL)
  {
    if(list_find(b, f, a->data) == NULL)
      return false;

    a = a->next;
  }

  return true;
}

bool list_equals(list_t* a, list_t* b, cmp_fn f)
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

bool list_any(list_t* list, pred_fn f, const void* arg)
{
  while(list != NULL)
  {
    if(f(list->data, (void*)arg))
      return true;

    list = list->next;
  }

  return false;
}

bool list_all(list_t* list, pred_fn f, const void* arg)
{
  while(list != NULL)
  {
    if(!f(list->data, (void*)arg))
      return false;

    list = list->next;
  }

  return true;
}

list_t* list_map(list_t* list, map_fn f, const void* arg)
{
  list_t* to = NULL;

  while(list != NULL)
  {
    void* result = f(list->data, (void*)arg);

    if(result != NULL)
      to = list_push(to, result);

    list = list->next;
  }

  return list_reverse(to);
}

list_t* list_reverse(list_t* list)
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

uint64_t list_hash(list_t* list, hash_fn f)
{
  uint64_t h = 0;

  while(list != NULL)
  {
    h ^= f(list->data);
    list = list->next;
  }

  return h;
}

size_t list_length(list_t* list)
{
  size_t len = 0;

  while(list != NULL)
  {
    len++;
    list = list->next;
  }

  return len;
}

void list_free(list_t* list, free_fn f)
{
  list_t* next;

  while(list != NULL)
  {
    next = list->next;

    if(f != NULL)
      f(list->data);

    free(list);
    list = next;
  }
}
