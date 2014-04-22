#include "list.h"
#include <stdlib.h>

struct list_t
{
  void* data;
  struct list_t* next;
};

list_t* list_push(list_t* list, const void* data)
{
  list_t* l = malloc(sizeof(list_t));
  l->data = (void*)data;
  l->next = list;

  return l;
}

list_t* list_append(list_t* list, const void* data)
{
  list_t* l = malloc(sizeof(list_t));
  l->data = (void*)data;
  l->next = NULL;

  if(list == NULL) return l;

  list_t* p = list;

  while(p->next != NULL)
  {
    p = p->next;
  }

  p->next = l;
  return list;
}

list_t* list_next(list_t* list)
{
  return list->next;
}

list_t* list_index(list_t* list, int index)
{
  if(index < 0)
  {
    index = list_length(list) + index;
  }

  for(int i = 0; (list != NULL) && (i < index); i++)
  {
    list = list->next;
  }

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
    if(f(data, list->data)) return list->data;
    list = list->next;
  }

  return NULL;
}

bool list_superset(list_t* list, list_t* sublist, cmp_fn f)
{
  while(sublist != NULL)
  {
    if(list_find(list, f, sublist->data) == NULL) return false;
    sublist = sublist->next;
  }

  return true;
}

bool list_equals(list_t* a, list_t* b, cmp_fn f)
{
  while(a != NULL)
  {
    if((b == NULL) || !f(a->data, b->data)) return false;
    a = a->next;
    b = b->next;
  }

  return b == NULL;
}

bool list_test(list_t* list, pred_fn f, void* arg)
{
  while(list != NULL)
  {
    if(!f(arg, list)) return false;
    list = list->next;
  }

  return true;
}

list_t* list_map(list_t* list, map_fn f, void* arg)
{
  list_t* to = NULL;

  while(list != NULL)
  {
    to = list_append(to, f(arg, list));
    list = list->next;
  }

  return to;
}

uint64_t list_hash(list_t* list, hash_fn f, uint64_t seed)
{
  while(list != NULL)
  {
    seed = f(list->data, seed);
    list = list->next;
  }

  return seed;
}

int list_length(list_t* list)
{
  int len = 0;

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
    if(f != NULL) f(list->data);
    free(list);
    list = next;
  }
}
