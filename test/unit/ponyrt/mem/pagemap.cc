#include <platform/platform.h>

PONY_EXTERN_C_BEGIN
#include <mem/pagemap.h>
PONY_EXTERN_C_END

#include <gtest/gtest.h>

TEST(Pagemap, UnmappedIsNull)
{
  ASSERT_EQ(NULL, pagemap_get((void*)0x0));
  ASSERT_EQ(NULL, pagemap_get((void*)0x7623432D));
  ASSERT_EQ(NULL, pagemap_get((void*)0xFFFFFFFFFFFFFFFF));
}

TEST(Pagemap, SetAndUnset)
{
  void* m = (void*)(44 << 12);

  pagemap_set(m, m);
  ASSERT_EQ(m, pagemap_get(m));

  pagemap_set(m, NULL);
  ASSERT_EQ(NULL, pagemap_get(m));
}

TEST(Pagemap, SubAddressing)
{
  char* m = (char*)(99 << 12);
  pagemap_set(m, m);

  char* p = m;
  char* end = p + 2048;

  while(p < end)
  {
    ASSERT_EQ(m, pagemap_get(p));
    p += 64;
  }

  ASSERT_EQ(NULL, pagemap_get(end));
}
