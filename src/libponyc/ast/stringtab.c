#include "stringtab.h"
#include "../../libponyrt/ds/hash.h"
#include "../../libponyrt/gc/serialise.h"
#include "../../libponyrt/mem/pool.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <platform.h>

static bool ptr_cmp(const char* a, const char* b)
{
  return a == b;
}

DEFINE_LIST(strlist, strlist_t, const char, ptr_cmp, NULL);

typedef struct stringtab_entry_t
{
  const char* str;
  size_t len;
  size_t buf_size;
} stringtab_entry_t;

static size_t stringtab_hash(stringtab_entry_t* a)
{
  return ponyint_hash_block(a->str, a->len);
}

static bool stringtab_cmp(stringtab_entry_t* a, stringtab_entry_t* b)
{
  return (a->len == b->len) && (memcmp(a->str, b->str, a->len) == 0);
}

static void stringtab_free(stringtab_entry_t* a)
{
  ponyint_pool_free_size(a->buf_size, (char*)a->str);
  POOL_FREE(stringtab_entry_t, a);
}

DECLARE_HASHMAP(strtable, strtable_t, stringtab_entry_t);
DEFINE_HASHMAP(strtable, strtable_t, stringtab_entry_t, stringtab_hash,
  stringtab_cmp, stringtab_free);

static strtable_t table;

void stringtab_init()
{
  strtable_init(&table, 4096);
}

const char* stringtab(const char* string)
{
  if(string == NULL)
    return NULL;

  return stringtab_len(string, strlen(string));
}

const char* stringtab_len(const char* string, size_t len)
{
  if(string == NULL)
    return NULL;

  stringtab_entry_t key = {string, len, 0};
  size_t index = HASHMAP_UNKNOWN;
  stringtab_entry_t* n = strtable_get(&table, &key, &index);

  if(n != NULL)
    return n->str;

  char* dst = (char*)ponyint_pool_alloc_size(len + 1);
  memcpy(dst, string, len);
  dst[len] = '\0';

  n = POOL_ALLOC(stringtab_entry_t);
  n->str = dst;
  n->len = len;
  n->buf_size = len + 1;

  // didn't find it in the map but index is where we can put the
  // new one without another search
  strtable_putindex(&table, n, index);
  return n->str;
}

const char* stringtab_consume(const char* string, size_t buf_size)
{
  if(string == NULL)
    return NULL;

  size_t len = strlen(string);
  stringtab_entry_t key = {string, len, 0};
  size_t index = HASHMAP_UNKNOWN;
  stringtab_entry_t* n = strtable_get(&table, &key, &index);

  if(n != NULL)
  {
    ponyint_pool_free_size(buf_size, (void*)string);
    return n->str;
  }

  n = POOL_ALLOC(stringtab_entry_t);
  n->str = string;
  n->len = len;
  n->buf_size = buf_size;

  // didn't find it in the map but index is where we can put the
  // new one without another search
  strtable_putindex(&table, n, index);
  return n->str;
}

void stringtab_done()
{
  strtable_destroy(&table);
  memset(&table, 0, sizeof(strtable_t));
}

static void string_serialise(pony_ctx_t* ctx, void* object, void* buf,
  uint64_t offset, int mutability)
{
  (void)ctx;
  (void)mutability;

  const char* string = (const char*)object;

  memcpy((void*)((uintptr_t)buf + (uintptr_t)offset), object,
    strlen(string) + 1);
}

static __pony_thread_local struct _pony_type_t string_pony =
{
  0,
  0,
  0,
  0,
  0,
  0,
  NULL,
  NULL,
  NULL,
  string_serialise,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  NULL,
  NULL,
  NULL
};

void string_trace(pony_ctx_t* ctx, const char* string)
{
  string_trace_len(ctx, string, strlen(string));
}

void string_trace_len(pony_ctx_t* ctx, const char* string, size_t len)
{
  string_pony.size = (uint32_t)(len + 1);
  pony_traceknown(ctx, (char*)string, &string_pony, PONY_TRACE_OPAQUE);
}

static void* string_deserialise(void* buf, size_t remaining_size)
{
  size_t len = 1;

  do
  {
    if(len >= remaining_size)
      return NULL;
  } while(((char*)buf)[len++] != '\0');

  return (void*)stringtab_len((const char*)buf, len - 1);
}

const char* string_deserialise_offset(pony_ctx_t* ctx, uintptr_t offset)
{
  return (const char*)pony_deserialise_raw(ctx, offset, string_deserialise);
}

static void strlist_serialise_trace(pony_ctx_t* ctx, void* object)
{
  strlist_t* list = (strlist_t*)object;

  if(list->contents.data != NULL)
    string_trace(ctx, (const char*)list->contents.data);

  if(list->contents.next != NULL)
    pony_traceknown(ctx, list->contents.next, strlist_pony_type(),
      PONY_TRACE_MUTABLE);
}

static void strlist_serialise(pony_ctx_t* ctx, void* object, void* buf,
  uint64_t offset, int mutability)
{
  (void)mutability;

  strlist_t* list = (strlist_t*)object;
  strlist_t* dst = (strlist_t*)((uintptr_t)buf + (uintptr_t)offset);

  dst->contents.data = (void*)pony_serialise_offset(ctx, list->contents.data);
  dst->contents.next = (list_t*)pony_serialise_offset(ctx, list->contents.next);

}

static void strlist_deserialise(pony_ctx_t* ctx, void* object)
{
  strlist_t* list = (strlist_t*)object;

  list->contents.data = (void*)string_deserialise_offset(ctx,
    (uintptr_t)list->contents.data);
  list->contents.next = (list_t*)pony_deserialise_offset(ctx,
    strlist_pony_type(), (uintptr_t)list->contents.next);
}

static pony_type_t strlist_pony =
{
  0,
  0,
  0,
  sizeof(strlist_t),
  0,
  0,
  NULL,
  NULL,
  strlist_serialise_trace,
  strlist_serialise,
  strlist_deserialise,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  NULL,
  NULL,
  NULL
};

pony_type_t* strlist_pony_type()
{
  return &strlist_pony;
}
