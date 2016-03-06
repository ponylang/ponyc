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
  void* m = (void*)(44 << 12);

  ponyint_pagemap_set(m, m);
  ASSERT_EQ(m, ponyint_pagemap_get(m));

  ponyint_pagemap_set(m, NULL);
  ASSERT_EQ(NULL, ponyint_pagemap_get(m));
}

TEST(Pagemap, SubAddressing)
{
  char* m = (char*)(99 << 12);
  ponyint_pagemap_set(m, m);

  char* p = m;
  char* end = p + 1024;

  while(p < end)
  {
    ASSERT_EQ(m, ponyint_pagemap_get(p));
    p += 64;
  }

  ASSERT_EQ(NULL, ponyint_pagemap_get(end));
}
