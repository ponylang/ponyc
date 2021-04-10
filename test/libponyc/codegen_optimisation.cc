#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "ir"))


class CodegenOptimisationTest : public PassTest
{};


TEST_F(CodegenOptimisationTest, MergeSendMessageReordering)
{
  const char* src =
    "use @pony_exitcode[None](code: I32)\n"
    "actor Main\n"
    "  var state: U8 = 0\n"
    "  new create(env: Env) =>\n"
    "    msg1(env)\n"
    "    msg2(env)\n"
    "    msg3(env)\n"

    "  be msg1(env: Env) =>\n"
    "    if state == 0 then state = 1 end\n"

    "  be msg2(env: Env) =>\n"
    "    if state == 1 then state = 2 end\n"

    "  be msg3(env: Env) =>\n"
    "    if state == 2 then\n"
    "      @pony_exitcode(I32(1))\n"
    "    end";

  opt.release = true;

  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 1);
}

TEST_F(CodegenOptimisationTest, MergeSendCrossMessaging)
{
  // Test that we don't produce invalid LLVM IR that would cause a crash.
  const char* src =
    "actor A\n"
    "  be m(a: A) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    test(A, A)\n"

    "  be test(a1: A, a2: A) =>\n"
    "    a1.m(a2)\n"
    "    a2.m(a1)";

  opt.release = true;

  TEST_COMPILE(src);
}
