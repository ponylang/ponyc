#include "table.h"
#include "list.h"
#include "hash.h"
#include <stdlib.h>

struct table_t
{
  size_t size;
  list_t** node;
};

table_t* table_create(size_t size)
{
  table_t* table = (table_t*)malloc(sizeof(table_t));
  table->size = next_pow2(size);
  table->node = (list_t**)calloc(table->size, sizeof(list_t*));

  return table;
}

bool table_apply(table_t* table, apply_fn apply, void* apply_arg)
{
  if(table == NULL)
    return false;

  for(int i = 0; i < table->size; i++)
  {
    list_t* list = table->node[i];

    while(list != NULL)
    {
      void* item = list_data(list);

      if(!apply(item, apply_arg))
        return false;

      list = list_next(list);
    }
  }

  return true;
}

bool table_merge(table_t* dst, table_t* src, hash_fn hash, cmp_fn cmp,
  dup_fn dup, pred_fn pred, void* pred_arg, resolve_fn resolve,
  void* resolve_arg)
{
  if(src == NULL)
    return true;

  if(dst == NULL)
    return false;

  bool ok = true;

  for(int i = 0; i < src->size; i++)
  {
    list_t* list = src->node[i];

    while(list != NULL)
    {
      void* item = list_data(list);

      if((pred == NULL) || pred(item, pred_arg))
      {
        bool present;
        void* prev = table_insert(dst, item, &present, hash, cmp, dup);

        if(present)
        {
          if(resolve != NULL)
            ok = resolve(prev, item, resolve_arg);
          else
            ok = false;
        }
      }

      list = list_next(list);
    }
  }

  return ok;
}

void* table_find(table_t* table, void* entry, hash_fn hash, cmp_fn cmp)
{
  uint64_t h = hash(entry) & (table->size - 1);
  return list_find(table->node[h], cmp, entry);
}

void* table_insert(table_t* table, void* entry, bool* present,
  hash_fn hash, cmp_fn cmp, dup_fn dup)
{
  uint64_t h = hash(entry) & (table->size - 1);
  void* data = list_find(table->node[h], cmp, entry);

  if(data == NULL)
  {
    *present = false;
    data = dup(entry);
    table->node[h] = list_push(table->node[h], data);
  } else {
    *present = true;
  }

  return data;
}

void table_free(table_t* table, free_fn fr)
{
  if(table == NULL)
    return;

  for(int i = 0; i < table->size; i++)
    list_free(table->node[i], fr);

  free(table->node);
  free(table);
}
