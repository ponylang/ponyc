#include <gtest/gtest.h>
#include <platform.h>

#include <codegen/gentype.h>
#include <../libponyrt/mem/pool.h>

#include "util.h"

#include "llvm_config_begin.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/Module.h>

#include "llvm_config_end.h"

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
  pony_exitcode((int)num_objects);
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
  // From issue #2245
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


TEST_F(CodegenTest, DoNotOptimiseApplyPrimitive)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    DoNotOptimise[I64](0)";

  TEST_COMPILE(src);
}


// Doesn't work on windows. The C++ code doesn't catch C++ exceptions if they've
// traversed a Pony frame. This is suspected to be related to how SEH and LLVM
// exception code generation interact.
// See https://github.com/ponylang/ponyc/issues/2455 for more details.
//
// This test is disabled on LLVM 3.9 and 4.0 because exceptions crossing JIT
// boundaries are broken with the ORC JIT on these versions.
#if (PONY_LLVM >= 500) && !defined(PLATFORM_IS_WINDOWS)
TEST_F(CodegenTest, TryBlockCantCatchCppExcept)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let r = @codegen_test_tryblock_catch[I32](this~tryblock())\n"
    "    @pony_exitcode[I32](r)\n"

    "  fun @tryblock() =>\n"
    "    try\n"
    "      @codegen_test_tryblock_throw[None]()?\n"
    "    else\n"
    "      None\n"
    "    end";

  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 1);
}


extern "C"
{

EXPORT_SYMBOL int codegen_test_tryblock_catch(void (*callback)())
{
  try
  {
    callback();
    return 0;
  } catch(std::exception const&) {
    return 1;
  }
}

EXPORT_SYMBOL void codegen_test_tryblock_throw()
{
  throw std::exception{};
}

}
#endif


TEST_F(CodegenTest, DescTable)
{
  const char* src =
    "class C1\n"
    "class C2\n"
    "class C3\n"
    "actor A1\n"
    "actor A2\n"
    "actor A3\n"
    "primitive P1\n"
    "primitive P2\n"
    "primitive P3\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"

  // Reach various types.

    "    (C1, A1, P1)\n"
    "    (C2, A2, P2)\n"
    "    (C3, A3, P3)\n"
    "    (C1, I8)\n"
    "    (C2, I16)\n"
    "    (C3, I32)";

  TEST_COMPILE(src);

  auto module = llvm::unwrap(compile->module);

  auto table_glob = module->getNamedGlobal("__DescTable");
  ASSERT_NE(table_glob, nullptr);
  ASSERT_TRUE(table_glob->hasInitializer());

  auto desc_table = llvm::dyn_cast_or_null<llvm::ConstantArray>(
    table_glob->getInitializer());
  ASSERT_NE(desc_table, nullptr);

  for(unsigned int i = 0; i < desc_table->getNumOperands(); i++)
  {
    // Check that for each element of the table, `desc_table[i]->id == i`.

    auto table_element = desc_table->getOperand(i);
    ASSERT_EQ(table_element->getType(), llvm::unwrap(compile->descriptor_ptr));

    if(table_element->isNullValue())
      continue;

    auto elt_bitcast = llvm::dyn_cast_or_null<llvm::ConstantExpr>(
      table_element);
    ASSERT_NE(elt_bitcast, nullptr);
    ASSERT_EQ(elt_bitcast->getOpcode(), llvm::Instruction::BitCast);

    auto desc_ptr = llvm::dyn_cast_or_null<llvm::GlobalVariable>(
      elt_bitcast->getOperand(0));
    ASSERT_NE(desc_ptr, nullptr);
    ASSERT_TRUE(desc_ptr->hasInitializer());

    auto desc = llvm::dyn_cast_or_null<llvm::ConstantStruct>(
      desc_ptr->getInitializer());
    ASSERT_NE(desc, nullptr);

    auto type_id = llvm::dyn_cast_or_null<llvm::ConstantInt>(
      desc->getOperand(0));
    ASSERT_NE(type_id, nullptr);

    ASSERT_EQ(type_id->getBitWidth(), 32);
    ASSERT_EQ(type_id->getZExtValue(), i);
  }
}


TEST_F(CodegenTest, RecoverCast)
{
  // From issue #2639
  const char* src =
    "class A\n"
    "  new create() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: ((None, None) | (A iso, None)) =\n"
    "      if false then\n"
    "        (None, None)\n"
    "      else\n"
    "        recover (A, None) end\n"
    "      end";

  TEST_COMPILE(src);
}

TEST_F(CodegenTest, VariableDeclNestedTuple)
{
  // From issue #2662
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let array = Array[((U8, U8), U8)]\n"
    "    for ((a, b), c) in array.values() do\n"
    "      None\n"
    "    end";

  TEST_COMPILE(src);
}

TEST_F(CodegenTest, TupleRemoveUnreachableBlockInLoop)
{
  // From issue #2760
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    var token = U8(1)\n"
    "    repeat\n"
    "      (token, let source) = (U8(1), U8(2))\n"
    "    until token == 2 end";

  TEST_COMPILE(src);
}

TEST_F(CodegenTest, TupleRemoveUnreachableBlock)
{
  // From issue #2735
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    if true then\n"
    "      (let f, let g) = (U8(2), U8(3))\n"
    "    end";

  TEST_COMPILE(src);
}

TEST_F(CodegenTest, RedundantUnionInForLoopIteratorValueType)
{
  // From issue #2736
  const char* src =
    "type T is (U8 | U8)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let i = I\n"
    "    for m in i do\n"
    "      None\n"
    "    end\n"

    "class ref I is Iterator[T]\n"
    "  new create() => None\n"
    "  fun ref has_next(): Bool => false\n"
    "  fun ref next(): T ? => error";

  TEST_COMPILE(src);
}

TEST_F(CodegenTest, WhileLoopBreakOnlyValueNeeded)
{
  // From issue #2788
  const char* src =
    "actor Main\n"
      "new create(env: Env) =>\n"
        "let x = while true do break true else false end\n"
        "if x then None end";

  TEST_COMPILE(src);
}

TEST_F(CodegenTest, WhileLoopBreakOnlyInBranches)
{
  // From issue #2788
  const char* src =
    "actor Main\n"
      "new create(env: Env) =>\n"
        "while true do\n"
          "if true then\n"
            "break None\n"
          "else\n"
            "break\n"
          "end\n"
        "else\n"
          "return\n"
        "end";

  TEST_COMPILE(src);
}

TEST_F(CodegenTest, RepeatLoopBreakOnlyValueNeeded)
{
  // From issue #2788
  const char* src =
    "actor Main\n"
      "new create(env: Env) =>\n"
        "let x = repeat break true until false else false end\n"
        "if x then None end";

  TEST_COMPILE(src);
}

TEST_F(CodegenTest, RepeatLoopBreakOnlyInBranches)
{
  // From issue #2788
  const char* src =
    "actor Main\n"
      "new create(env: Env) =>\n"
        "repeat\n"
          "if true then\n"
            "break None\n"
          "else\n"
            "break\n"
          "end\n"
        "until\n"
          "false\n"
        "else\n"
          "return\n"
        "end";

  TEST_COMPILE(src);
}

TEST_F(CodegenTest, CycleDetector)
{
  const char* src =
    "use @printf[I32](fmt: Pointer[U8] tag, ...)\n"

    "actor Ring\n"
    "  let _id: U32\n"
    "  var _next: (Ring | None)\n"

    "  new create(id: U32, neighbor: (Ring | None) = None) =>\n"
    "    _id = id\n"
    "    _next = neighbor\n"

    "  be set(neighbor: Ring) =>\n"
    "    _next = neighbor\n"

    "  be pass(i: USize) =>\n"
    "    if i > 0 then\n"
    "      match _next\n"
    "      | let n: Ring =>\n"
    "        n.pass(i - 1)\n"
    "      end\n"
    "    end\n"

    "  fun _final() =>\n"
    "    let i = @pony_get_exitcode[I32]()\n"
    "    @pony_exitcode[None](i + 1)\n"

    "actor Watcher\n"
    "  var _c: I32 = 0\n"
    "  new create(num: I32) => check_done(num)\n"

    "  be check_done(num: I32) =>\n"
    "    if @pony_get_exitcode[I32]() != num then\n"
    "      /* wait for cycle detector to reap ring actors */\n"
    "      ifdef windows then\n"
    "        @Sleep[None](U32(30))\n"
    "      else\n"
    "        @ponyint_cpu_core_pause[None](U64(0), U64(20000000000), true)\n"
    "      end\n"
    "      _c = _c + 1\n"
    "      if _c > 50 then\n"
    "        @printf(\"Ran out of time... exit count is: %d instead of %d\n\".cstring(), @pony_get_exitcode[I32](), num)\n"
    "        @pony_exitcode[None](I32(1))\n"
    "      else\n"
    "        check_done(num)\n"
    "      end\n"
    "    else\n"
    "      @pony_exitcode[None](I32(0))\n"
    "    end\n"


    "actor Main\n"
    "  var _ring_size: U32 = 100\n"
    "  var _ring_count: U32 = 100\n"
    "  var _pass: USize = 10\n"

    "  new create(env: Env) =>\n"
    "    setup_ring()\n"
    "    Watcher((_ring_size * _ring_count).i32())\n"

    "  fun setup_ring() =>\n"
    "    var j: U32 = 0\n"
    "    while true do\n"
    "      let first = Ring(1)\n"
    "      var next = first\n"

    "      var k: U32 = 0\n"
    "      while true do\n"
    "        let current = Ring(_ring_size - k, next)\n"
    "        next = current\n"

    "        k = k + 1\n"
    "        if k >= (_ring_size - 1) then\n"
    "          break\n"
    "        end\n"
    "      end\n"

    "      first.set(next)\n"

    "      if _pass > 0 then\n"
    "        first.pass(_pass)\n"
    "      end\n"

    "      j = j + 1\n"
    "      if j >= _ring_count then\n"
    "        break\n"
    "      end\n"
    "    end\n";

  set_builtin(NULL);

  TEST_COMPILE(src);

  int exit_code = -1;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 0);
}

TEST_F(CodegenTest, TryThenClauseReturn)
{
  const char * src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try\n"
    "      if false then error end\n"
    "      return\n"
    "    then\n"
    "      @pony_exitcode[None](I32(42))"
    "    end\n"
    "    @pony_exitcode[None](I32(0))";

  TEST_COMPILE(src);

  int exit_code = -1;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 42);
}

TEST_F(CodegenTest, TryThenClauseReturnNested)
{
  const char * src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try\n"
    "      try\n"
    "        if false then error end\n"
    "        return\n"
    "      end\n"
    "    then\n"
    "      @pony_exitcode[None](I32(42))"
    "    end\n"
    "    @pony_exitcode[None](I32(0))";

  TEST_COMPILE(src);

  int exit_code = -1;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 42);
}

TEST_F(CodegenTest, TryThenClauseBreak)
{
  const char * src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    var r: I32 = 0\n"
    "    while true do\n"
    "      try\n"
    "        if Bool(false) then error end\n"
    "        break\n"
    "      then\n"
    "        r = 42\n"
    "      end\n"
    "    end\n"
    "    @pony_exitcode[None](r)";
  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 42);
}

TEST_F(CodegenTest, TryThenClauseBreakNested)
{
  const char * src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    var r: I32 = 0\n"
    "    while true do\n"
    "      try\n"
    "        try\n"
    "          if Bool(false) then error end\n"
    "          break\n"
    "        end\n"
    "      then\n"
    "        r = 42\n"
    "      end\n"
    "    end\n"
    "    @pony_exitcode[None](r)";
  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 42);
}

TEST_F(CodegenTest, TryThenClauseContinue)
{
  const char * src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    var r: I32 = 0\n"
    "    while r == 0 do\n"
    "      try\n"
    "        if Bool(false) then error else r = 1 end\n"
    "        continue\n"
    "      then\n"
    "        r = 42\n"
    "      end\n"
    "    end\n"
    "    @pony_exitcode[None](r)";
  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 42);
}

TEST_F(CodegenTest, TryThenClauseContinueNested)
{
  const char * src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    var r: I32 = 0\n"
    "    while r == 0 do\n"
    "      try\n"
    "        if Bool(true) then\n"
    "          try\n"
    "            if Bool(false) then error else r = 1 end\n"
    "            continue\n"
    "          end\n"
    "        end\n"
    "      then\n"
    "        r = 42\n"
    "      end\n"
    "    end\n"
    "    @pony_exitcode[None](r)";
  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 42);
}

TEST_F(CodegenTest, IfBlockEndingWithEmbedAssign)
{
  // From issue #3669
  const char* src =
    "class B\n"
    "class A\n"
    "  embed embedded: B\n"
    "  new create() => \n"
    "    if true\n"
    "    then embedded = B\n"
    "    else embedded = B\n"
    "    end\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    A";

  TEST_COMPILE(src);

  // LLVM Module verification fails if bug is present:
  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
}

TEST_F(CodegenTest, IfBlockEndingWithDontCareAssign)
{
  // From issue #3669
  const char* src =
    "class B\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    if true\n"
    "    then _ = B\n"
    "    end";

  TEST_COMPILE(src);

  // LLVM Module verification fails if bug is present:
  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
}

TEST_F(CodegenTest, UnionValueForTupleReturnType)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try _create_tuple()._2 as Bool end\n"

    "  fun _create_tuple(): (U32, (Bool | None)) =>\n"
    "    let x: ((U32, Bool) | (U32, None)) = (1, true)\n"
    "    x\n";

  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 0);
}
