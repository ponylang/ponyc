#include <platform.h>

#include <gc/image.h>

#include <gtest/gtest.h>
#include <stdlib.h>

// The GC skips objects in memory that is read-only once the program is
// loaded, so that memory must not include anything a program can write.

namespace
{
const uint64_t constant_value = 7;

// A constant holding a pointer, which the loader must relocate.
const uint64_t* const constant_pointer = &constant_value;

uint64_t writable_value = 7;
uint64_t zeroed_value;
}

TEST(Image, ConstantsAreReadOnly)
{
  ponyint_image_init();

  ASSERT_TRUE(ponyint_image_constant(&constant_value));
  ASSERT_TRUE(ponyint_image_constant(&constant_pointer));
}

TEST(Image, WritableMemoryIsNotReadOnly)
{
  ponyint_image_init();

  uint64_t local = 7;
  void* heap = malloc(64);

  ASSERT_FALSE(ponyint_image_constant(&writable_value));
  ASSERT_FALSE(ponyint_image_constant(&zeroed_value));
  ASSERT_FALSE(ponyint_image_constant(&local));
  ASSERT_FALSE(ponyint_image_constant(heap));

  free(heap);
}
