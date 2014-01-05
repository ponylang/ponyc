#include "table.h"
#include "list.h"
#include <stdlib.h>

struct table_t
{
  int size;
  int mask;
  hash_fn hash;
  cmp_fn cmp;
  dup_fn dup;
  free_fn fr;
  list_t** node;
};

static int next_pow2(int v)
{
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  return v + 1;
}

table_t* table_create(int size, hash_fn hash, cmp_fn cmp, dup_fn dup, free_fn fr)
{
  table_t* table = malloc(sizeof(table_t));
  table->size = next_pow2(size);
  table->mask = table->size - 1;
  table->hash = hash;
  table->cmp = cmp;
  table->dup = dup;
  table->fr = fr;
  table->node = calloc(table->size, sizeof(list_t*));

  return table;
}

bool table_merge(table_t* dst, const table_t* src)
{
  if(src == NULL) return true;
  if(dst == NULL) return false;
  bool found = false;

  for(int i = 0; i < src->size; i++)
  {
    list_t* list = src->node[i];

    while(list != NULL)
    {
      bool present;
      table_insert(dst, list_data(list), &present);
      found |= present;
      list = list_next(list);
    }
  }

  return !found;
}

void* table_find(const table_t* table, void* entry)
{
  uint64_t hash = table->hash(entry, 0) & table->mask;
  return list_find(table->node[hash], table->cmp, entry);
}

void* table_insert(table_t* table, void* entry, bool* present)
{
  uint64_t hash = table->hash(entry, 0) & table->mask;
  void* data = list_find(table->node[hash], table->cmp, entry);

  if(data == NULL)
  {
    *present = false;
    data = table->dup(entry);
    table->node[hash] = list_push(table->node[hash], data);
  } else {
    *present = true;
  }

  return data;
}

void table_free(table_t* table)
{
  if(table == NULL) return;

  for(int i = 0; i < table->size; i++)
    list_free(table->node[i], table->fr);

  free(table->node);
  free(table);
}
