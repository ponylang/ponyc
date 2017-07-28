#include <platform.h>

#include <mem/pagemap.h>

#include <gtest/gtest.h>

TEST(Pagemap, UnmappedIsNull)
{
  ASSERT_EQ(NULL, ponyint_pagemap_get((void*)0x0));
  ASSERT_EQ(NULL, ponyint_pagemap_get((void*)0x7623432D));
  ASSERT_EQ(NULL, ponyint_pagemap_get((void*)0xFFFFFFFFFFFFFFFF));
}

TEST(Pagemap, SetAndUnset)
{
  char* m = (char*)(44 << 12);

  ponyint_pagemap_set(m, (chunk_t*)m);
  ASSERT_EQ(m, (char*)ponyint_pagemap_get(m));

  ponyint_pagemap_set(m, NULL);
  ASSERT_EQ(NULL, ponyint_pagemap_get(m));
}

TEST(Pagemap, SubAddressing)
{
  char* m = (char*)(99 << 12);
  ponyint_pagemap_set(m, (chunk_t*)m);

  char* p = m;
  char* end = p + 1024;

  while(p < end)
  {
    ASSERT_EQ(m, (char*)ponyint_pagemap_get((chunk_t*)p));
    p += 64;
  }

  ASSERT_EQ(NULL, ponyint_pagemap_get((chunk_t*)end));
}

TEST(Pagemap, SetAndUnsetBulk)
{
  char* m = (char*)(155 << 12);
  size_t size = 65536;

  ponyint_pagemap_set_bulk(m, (chunk_t*)m, size);

  char* p = m;
  char* end = p + size;

  while(p < end)
  {
    ASSERT_EQ(m, (char*)ponyint_pagemap_get((chunk_t*)p));
    p += 1024;
  }

  ASSERT_EQ(NULL, ponyint_pagemap_get((chunk_t*)end));

  ponyint_pagemap_set_bulk(m, NULL, size);

  p = m;

  while(p < end)
  {
    ASSERT_EQ(NULL, (char*)ponyint_pagemap_get((chunk_t*)p));
    p += 1024;
  }
}
