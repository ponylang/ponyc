#include <gtest/gtest.h>
#include <platform.h>

#include <codegen/gentype.h>
#include <../libponyrt/mem/pool.h>

#include "util.h"

#ifdef _MSC_VER
// Stop MSVC from complaining about conversions from LLVMBool to bool.
# pragma warning(disable:4800)
#endif

#define TEST_COMPILE(src) DO(test_compile(src, "ir"))


class CodegenTest : public PassTest
{
};


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

  LLVMTypeRef type = ((compile_type_t*)foo->c_type)->structure;
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

  LLVMTypeRef type = ((compile_type_t*)foo->c_type)->structure;
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

  LLVMTypeRef type = ((compile_type_t*)foo->c_type)->structure;
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

  LLVMTypeRef type = ((compile_type_t*)foo->c_type)->structure;
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


TEST_F(CodegenTest, BoxBoolAsUnionIntoTuple)
{
  // From #2191
  const char* src =
    "type U is (Bool | C)\n"

    "class C\n"
    "  fun apply(): (I32, U) =>\n"
    "    (0, false)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    C()";

  TEST_COMPILE(src);

  ASSERT_TRUE(compile != NULL);
}


extern "C"
{

static uint32_t num_objects = 0;

EXPORT_SYMBOL void codegentest_small_finalisers_increment_num_objects() {
  num_objects++;
  pony_exitcode(num_objects);
}

}


TEST_F(CodegenTest, SmallFinalisers)
{
  const char* src =
    "class _Final\n"
    "  fun _final() =>\n"
    "    @codegentest_small_finalisers_increment_num_objects[None]()\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    var i: U64 = 0\n"
    "    while i < 42 do\n"
    "      _Final\n"
    "      i = i + 1\n"
    "    end";

  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 42);
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

TEST_F(CodegenTest, MatchExhaustiveAllCasesOfUnion)
{
  const char* src =
    "class C1 fun one(): I32 => 1\n"
    "class C2 fun two(): I32 => 2\n"
    "class C3 fun three(): I32 => 3\n"

    "primitive Foo\n"
    "  fun apply(c': (C1 | C2 | C3)): I32 =>\n"
    "    match c'\n"
    "    | let c: C1 => c.one()\n"
    "    | let c: C2 => c.two()\n"
    "    | let c: C3 => c.three()\n"
    "    end\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @pony_exitcode[None](Foo(C3))";

  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 3);
}


TEST_F(CodegenTest, MatchExhaustiveAllCasesIncludingDontCareAndTuple)
{
  const char* src =
    "class C1 fun one(): I32 => 1\n"
    "class C2 fun two(): I32 => 2\n"
    "class C3 fun three(): I32 => 3\n"

    "primitive Foo\n"
    "  fun apply(c': (C1 | C2 | (C3, Bool))): I32 =>\n"
    "    match c'\n"
    "    | let c: C1 => c.one()\n"
    "    | let _: C2 => 2\n"
    "    | (let c: C3, let _: Bool) => c.three()\n"
    "    end\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @pony_exitcode[None](Foo((C3, true)))";
  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 3);
}


TEST_F(CodegenTest, MatchExhaustiveAllCasesPrimitiveValues)
{
  const char* src =
    "primitive P1 fun one(): I32 => 1\n"
    "primitive P2 fun two(): I32 => 2\n"
    "primitive P3 fun three(): I32 => 3\n"

    "primitive Foo\n"
    "  fun apply(p': (P1 | P2 | P3)): I32 =>\n"
    "    match p'\n"
    "    | let p: P1 => p.one()\n"
    "    | let p: P2 => p.two()\n"
    "    | let p: P3 => p.three()\n"
    "    end\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @pony_exitcode[None](Foo(P3))";

  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 3);
}


TEST_F(CodegenTest, ArrayInfersMostSpecificFromUnionOfArrayTypes)
{
  const char* src =
    "trait iso T1\n"
    "trait iso T2\n"
    "trait iso T3 is T1\n"
    "trait iso T4 is T2\n"
    "class iso C1 is T3\n"
    "class iso C2 is T4\n"

    "primitive Foo\n"
    "  fun apply(): (Array[T1] | Array[T2] | Array[T3] | Array[T4]) =>\n"
    "    [C1]\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    match Foo() \n"
    "    | let a: Array[T1] => @pony_exitcode[None](I32(1))\n"
    "    | let a: Array[T2] => @pony_exitcode[None](I32(2))\n"
    "    | let a: Array[T3] => @pony_exitcode[None](I32(3))\n"
    "    | let a: Array[T4] => @pony_exitcode[None](I32(4))\n"
    "    end";

  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 3);
}


TEST_F(CodegenTest, UnionOfTuplesToTuple)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a: ((Main, Env) | (Env, Main)) = (this, env)\n"
    "    let b: ((Main | Env), (Main | Env)) = a";

  TEST_COMPILE(src);
}


TEST_F(CodegenTest, ViewpointAdaptedFieldReach)
{
  // From issue #2238
  const char* src =
    "class A\n"
    "class B\n"
    "  var f: (A | None) = None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let l = B\n"
    "    l.f = A";

  TEST_COMPILE(src);
}


TEST_F(CodegenTest, StringSerialization)
{
  // From issue 2245
  const char* src =
    "use \"serialise\"\n"

    "class V\n"
    "  let _v: String = \"\"\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try\n"
    "      let auth = env.root as AmbientAuth\n"
    "      let v: V = V\n"
    "      Serialised(SerialiseAuth(auth), v)?\n"
    "      @pony_exitcode[None](I32(1))\n"
    "    end";

  set_builtin(NULL);

  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 1);
}


TEST_F(CodegenTest, CustomSerialization)
{
  const char* src =
    "use \"serialise\"\n"

    "class _Custom\n"
    "  let s1: String = \"abc\"\n"
    "  var p: Pointer[U8]\n"
    "  let s2: String = \"efg\"\n"

    "  new create() =>\n"
    "    p = @test_custom_serialisation_get_object[Pointer[U8] ref]()\n"

    "  fun _final() =>\n"
    "    @test_custom_serialisation_free_object[None](p)\n"

    "  fun _serialise_space(): USize =>\n"
    "    8\n"

    "  fun _serialise(bytes: Pointer[U8]) =>\n"
    "    @test_custom_serialisation_serialise[None](p, bytes)\n"

    "  fun ref _deserialise(bytes: Pointer[U8]) =>\n"
    "    p = @test_custom_serialisation_deserialise[Pointer[U8] ref](bytes)\n"

    "  fun eq(c: _Custom): Bool =>\n"
    "    (@test_custom_serialisation_compare[U8](this.p, c.p) == 1) and\n"
    "    (this.s1 == c.s1)\n"
    "      and (this.s2 == c.s2)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try\n"
    "      let ambient = env.root as AmbientAuth\n"
    "      let serialise = SerialiseAuth(ambient)\n"
    "      let deserialise = DeserialiseAuth(ambient)\n"

    "      let x: _Custom = _Custom\n"
    "      let sx = Serialised(serialise, x)?\n"
    "      let yd: Any ref = sx(deserialise)?\n"
    "      let y = yd as _Custom\n"
    "      let r: I32 = if (x isnt y) and (x == y) then\n"
    "        1\n"
    "      else\n"
    "        0\n"
    "      end\n"
    "      @pony_exitcode[None](r)\n"
    "    end";

  set_builtin(NULL);

  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 1);
}


extern "C"
{

EXPORT_SYMBOL void* test_custom_serialisation_get_object()
{
  uint64_t* i = POOL_ALLOC(uint64_t);
  *i = 0xDEADBEEF10ADBEE5;
  return i;
}

EXPORT_SYMBOL void test_custom_serialisation_free_object(uint64_t* p)
{
  POOL_FREE(uint64_t, p);
}

EXPORT_SYMBOL void test_custom_serialisation_serialise(uint64_t* p,
  unsigned char* bytes)
{
  *(uint64_t*)(bytes) = *p;
}

EXPORT_SYMBOL void* test_custom_serialisation_deserialise(unsigned char* bytes)
{
  uint64_t* p = POOL_ALLOC(uint64_t);
  *p = *(uint64_t*)(bytes);
  return p;
}

EXPORT_SYMBOL char test_custom_serialisation_compare(uint64_t* p1, uint64_t* p2)
{
  return *p1 == *p2;
}

}
