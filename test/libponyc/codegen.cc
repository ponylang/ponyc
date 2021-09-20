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

TEST_F(CodegenTest, CustomSerialization)
{
  const char* src =
    "use \"serialise\"\n"
    "use @pony_exitcode[None](code: I32)\n"
    "use @test_custom_serialisation_get_object[Pointer[U8] ref]()\n"
    "use @test_custom_serialisation_free_object[None](p: Pointer[U8] tag)\n"
    "use @test_custom_serialisation_serialise[None](p: Pointer[U8] tag, bytes: Pointer[U8] ref)\n"
    "use @test_custom_serialisation_deserialise[Pointer[U8] ref](byres: Pointer[U8] ref)\n"
    "use @test_custom_serialisation_compare[U8](p1: Pointer[U8] tag, p2: Pointer[U8] tag)\n"

    "class _Custom\n"
    "  let s1: String = \"abc\"\n"
    "  var p: Pointer[U8]\n"
    "  let s2: String = \"efg\"\n"

    "  new create() =>\n"
    "    p = @test_custom_serialisation_get_object()\n"

    "  fun _final() =>\n"
    "    @test_custom_serialisation_free_object(p)\n"

    "  fun _serialise_space(): USize =>\n"
    "    8\n"

    "  fun _serialise(bytes: Pointer[U8]) =>\n"
    "    @test_custom_serialisation_serialise(p, bytes)\n"

    "  fun ref _deserialise(bytes: Pointer[U8]) =>\n"
    "    p = @test_custom_serialisation_deserialise(bytes)\n"

    "  fun eq(c: _Custom): Bool =>\n"
    "    (@test_custom_serialisation_compare(this.p, c.p) == 1) and\n"
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
    "      @pony_exitcode(r)\n"
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
