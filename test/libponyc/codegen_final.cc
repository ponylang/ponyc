#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "ir"))


class CodegenFinalTest : public PassTest
{};


TEST_F(CodegenFinalTest, PrimitiveInit)
{
  const char* src =
    "use @pony_exitcode[None](code: I32)\n"
    "primitive PrimitiveInit\n"
    "  fun _init() =>\n"
    "    @pony_exitcode(I32(1))\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 1);
}


TEST_F(CodegenFinalTest, PrimitiveFinal)
{
  const char* src =
    "use @pony_exitcode[None](code: I32)\n"
    "primitive PrimitiveFinal\n"
    "  fun _final() =>\n"
    "    @pony_exitcode(I32(1))\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 1);
}


TEST_F(CodegenFinalTest, ClassFinal)
{
  const char* src =
    "use @pony_exitcode[None](code: I32)\n"
    "class ClassFinal\n"
    "  fun _final() =>\n"
    "    @pony_exitcode(I32(1))\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    ClassFinal";

  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 1);
}


TEST_F(CodegenFinalTest, EmbedFinal)
{
  const char* src =
    "use @pony_exitcode[None](code: I32)\n"
    "class EmbedFinal\n"
    "  fun _final() =>\n"
    "    @pony_exitcode(I32(1))\n"

    "actor Main\n"
    "  embed c: EmbedFinal = EmbedFinal\n"

    "  new create(env: Env) =>\n"
    "    None";

  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 1);
}


TEST_F(CodegenFinalTest, ActorFinal)
{
  const char* src =
    "use @pony_exitcode[None](code: I32)\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None\n"

    "  fun _final() =>\n"
    "    @pony_exitcode(I32(1))";

  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 1);
}
