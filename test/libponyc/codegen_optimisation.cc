#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "ir"))


class CodegenOptimisationTest : public PassTest
{};


TEST_F(CodegenOptimisationTest, MergeTraceMessageReordering)
{
  const char* src =
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
    "      @pony_exitcode[None](I32(1))\n"
    "    end";

  opt.release = true;

  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 1);
}
