#include <platform.h>

#include <mem/alloc.h>
#include <mem/pagemap.h>
#include <mem/pool.h>

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

TEST(Pagemap, LookupReturnsTheMappingForEachAddress)
{
  // addresses[3] must be in a different leaf than base, so distance must be at
  // least the span of a leaf (1 << L2_SHIFT in src/libponyrt/mem/pagemap.c).
  const size_t distance = 8 << 20;
  void* reservation = ponyint_virt_reserve_aligned(2 * distance);
  ASSERT_NE(nullptr, reservation);
  const uintptr_t base = (uintptr_t)reservation;
  const uintptr_t addresses[] =
    {base, base + POOL_ALIGN, base + (2 << 20), base + distance};
  uintptr_t chunks[4];
  uintptr_t actors[4];

  for(size_t i = 0; i < 3; i++)
    ponyint_pagemap_set((void*)addresses[i], (chunk_t*)&chunks[i],
      (pony_actor_t*)&actors[i]);

  pony_actor_t* owner = NULL;
  EXPECT_EQ((chunk_t*)&chunks[0], ponyint_pagemap_get(reservation, &owner));
  EXPECT_EQ(nullptr, ponyint_pagemap_get((void*)addresses[3], &owner));
  ponyint_pagemap_set((void*)addresses[3], (chunk_t*)&chunks[3],
    (pony_actor_t*)&actors[3]);
  EXPECT_EQ((chunk_t*)&chunks[3], ponyint_pagemap_get((void*)addresses[3],
    &owner));
  EXPECT_EQ((pony_actor_t*)&actors[3], owner);

  for(size_t i : {0u, 1u, 2u, 0u, 3u, 0u})
  {
    pony_actor_t* actor = NULL;
    EXPECT_EQ((chunk_t*)&chunks[i], ponyint_pagemap_get((void*)addresses[i],
      &actor));
    EXPECT_EQ((pony_actor_t*)&actors[i], actor);
  }

  ponyint_pagemap_set(reservation, (chunk_t*)&chunks[1],
    (pony_actor_t*)&actors[2]);
  EXPECT_EQ((chunk_t*)&chunks[1], ponyint_pagemap_get(reservation, &owner));
  EXPECT_EQ((pony_actor_t*)&actors[2], owner);

  for(uintptr_t address : addresses)
    ponyint_pagemap_set((void*)address, NULL, NULL);
  ponyint_virt_free(reservation, 2 * distance);
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
