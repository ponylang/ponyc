#include <platform.h>

#include <mem/pagemap.h>

#include <gtest/gtest.h>

TEST(Pagemap, UnmappedIsNull)
{
  pony_actor_t* actor = NULL;
  ASSERT_EQ(NULL, ponyint_pagemap_get((void*)0x0, &actor));
  ASSERT_EQ(NULL, actor);
  ASSERT_EQ(NULL, ponyint_pagemap_get((void*)0x7623432D, &actor));
  ASSERT_EQ(NULL, actor);
  ASSERT_EQ(NULL, ponyint_pagemap_get((void*)0xFFFFFFFFFFFFFFFF, &actor));
  ASSERT_EQ(NULL, actor);
}

TEST(Pagemap, SetAndUnset)
{
  char* m = (char*)(44 << 12);
  pony_actor_t* actor = (pony_actor_t*)0xABCD;
  pony_actor_t* pagemap_actor = NULL;

  ponyint_pagemap_set(m, (chunk_t*)m, actor);
  ASSERT_EQ(m, (char*)ponyint_pagemap_get(m, &pagemap_actor));
  ASSERT_EQ(actor, pagemap_actor);
  ASSERT_EQ(m, (char*)ponyint_pagemap_get_chunk(m));

  ponyint_pagemap_set(m, NULL, NULL);
  ASSERT_EQ(NULL, ponyint_pagemap_get(m, &pagemap_actor));
  ASSERT_EQ(NULL, pagemap_actor);
  ASSERT_EQ(NULL, ponyint_pagemap_get_chunk(m));
}

TEST(Pagemap, SubAddressing)
{
  char* m = (char*)(99 << 12);
  ponyint_pagemap_set(m, (chunk_t*)m, (pony_actor_t*)0xABCD);

  char* p = m;
  char* end = p + 1024;
  pony_actor_t* actor = NULL;

  while(p < end)
  {
    ASSERT_EQ(m, (char*)ponyint_pagemap_get((chunk_t*)p, &actor));
    ASSERT_EQ((pony_actor_t*)0xABCD, actor);
    ASSERT_EQ(m, (char*)ponyint_pagemap_get_chunk((chunk_t*)p));
    p += 64;
  }

  ASSERT_EQ(NULL, ponyint_pagemap_get((chunk_t*)end, &actor));
  ASSERT_EQ(NULL, actor);
  ASSERT_EQ(NULL, ponyint_pagemap_get_chunk((chunk_t*)end));
}

TEST(Pagemap, SetAndUnsetBulk)
{
  char* m = (char*)(155 << 12);
  size_t size = 8388608;

  ponyint_pagemap_set_bulk(m, (chunk_t*)m, (pony_actor_t*)0xABCD, size);

  char* p = m;
  char* end = p + size;
  pony_actor_t* actor = NULL;

  while(p < end)
  {
    ASSERT_EQ(m, (char*)ponyint_pagemap_get((chunk_t*)p, &actor));
    ASSERT_EQ((pony_actor_t*)0xABCD, actor);
    ASSERT_EQ(m, (char*)ponyint_pagemap_get_chunk((chunk_t*)p));
    p += 1024;
  }

  ASSERT_EQ(NULL, ponyint_pagemap_get((chunk_t*)end, &actor));
  ASSERT_EQ(NULL, actor);
  ASSERT_EQ(NULL, ponyint_pagemap_get_chunk((chunk_t*)end));

  ponyint_pagemap_set_bulk(m, NULL, NULL, size);

  p = m;

  while(p < end)
  {
    ASSERT_EQ(NULL, (char*)ponyint_pagemap_get((chunk_t*)p, &actor));
    ASSERT_EQ(NULL, actor);
    ASSERT_EQ(NULL, (char*)ponyint_pagemap_get_chunk((chunk_t*)p));
    p += 1024;
  }
}
