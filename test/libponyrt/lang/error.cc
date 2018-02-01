#include <platform.h>
#include <gtest/gtest.h>

#include <pony.h>

#include <exception>

TEST(ErrorTest, PonyTry)
{
  auto cb_success = [](void*){};
  auto cb_error = [](void*){ pony_error(); };

  ASSERT_TRUE(pony_try(cb_success, nullptr));
  ASSERT_FALSE(pony_try(cb_error, nullptr));
}

// Doesn't work on windows. The C++ code doesn't catch C++ exceptions if they've
// traversed a pony_try() call. This is suspected to be related to how SEH and
// LLVM exception code generation interact.
// See https://github.com/ponylang/ponyc/issues/2455 for more details.
#ifndef PLATFORM_IS_WINDOWS
TEST(ErrorTest, PonyTryCantCatchCppExcept)
{
  auto cb_cppthrow = [](void*){ throw std::exception{}; };

  ASSERT_THROW((pony_try(cb_cppthrow, nullptr)), std::exception);
}
#endif
