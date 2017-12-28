#include <platform.h>
#include <gtest/gtest.h>

#include <pony.h>

TEST(ErrorTest, PonyTry)
{
  auto cb_success = [](void*){};
  auto cb_error = [](void*){ pony_error(); };

  ASSERT_TRUE(pony_try(cb_success, nullptr));
  ASSERT_FALSE(pony_try(cb_error, nullptr));
}
