#include <gtest/gtest.h>
#include <platform.h>

#include <reach/reach.h>

#include "util.h"

#ifdef _MSC_VER
// Stop MSVC from complaining about conversions from LLVMBool to bool.
# pragma warning(disable:4800)
#endif

#define TEST_COMPILE(src) DO(test_compile(src, "ir"))


class CodegenTest : public PassTest
{};


TEST_F(CodegenTest, PackedStructIsPacked)
{
  const char* src =
    "struct \\packed\\ Foo\n"
    "  var a: U8 = 0\n"
    "  var b: U32 = 0\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Foo";

  TEST_COMPILE(src);

  reach_t* reach = compile->reach;
  reach_type_t* foo = reach_type_name(reach, "Foo");
  ASSERT_TRUE(foo != NULL);

  LLVMTypeRef type = foo->structure;
  ASSERT_TRUE(LLVMIsPackedStruct(type));
}


TEST_F(CodegenTest, NonPackedStructIsntPacked)
{
  const char* src =
    "struct Foo\n"
    "  var a: U8 = 0\n"
    "  var b: U32 = 0\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Foo";

  TEST_COMPILE(src);

  reach_t* reach = compile->reach;
  reach_type_t* foo = reach_type_name(reach, "Foo");
  ASSERT_TRUE(foo != NULL);

  LLVMTypeRef type = foo->structure;
  ASSERT_TRUE(!LLVMIsPackedStruct(type));
}


TEST_F(CodegenTest, ClassCannotBePacked)
{
  const char* src =
    "class \\packed\\ Foo\n"
    "  var a: U8 = 0\n"
    "  var b: U32 = 0\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Foo";

  TEST_COMPILE(src);

  reach_t* reach = compile->reach;
  reach_type_t* foo = reach_type_name(reach, "Foo");
  ASSERT_TRUE(foo != NULL);

  LLVMTypeRef type = foo->structure;
  ASSERT_TRUE(!LLVMIsPackedStruct(type));
}


TEST_F(CodegenTest, ActorCannotBePacked)
{
  const char* src =
    "actor \\packed\\ Foo\n"
    "  var a: U8 = 0\n"
    "  var b: U32 = 0\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Foo";

  TEST_COMPILE(src);

  reach_t* reach = compile->reach;
  reach_type_t* foo = reach_type_name(reach, "Foo");
  ASSERT_TRUE(foo != NULL);

  LLVMTypeRef type = foo->structure;
  ASSERT_TRUE(!LLVMIsPackedStruct(type));
}


TEST_F(CodegenTest, JitRun)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @pony_exitcode[None](I32(1))";

  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 1);
}


extern "C"
{

typedef int (*codegentest_ccallback_fn)(void* self, int value);

EXPORT_SYMBOL int codegentest_ccallback(void* self, codegentest_ccallback_fn cb,
  int value)
{
  return cb(self, value);
}

}


TEST_F(CodegenTest, CCallback)
{
  const char* src =
    "class Callback\n"
    "  fun apply(value: I32): I32 =>\n"
    "    value * 2\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let cb: Callback = Callback\n"
    "    let r = @codegentest_ccallback[I32](cb, addressof cb.apply, I32(3))\n"
    "    @pony_exitcode[None](r)";

  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 6);
}
