#include <gtest/gtest.h>
#include <platform.h>

#include <codegen/gentype.h>

#include "util.h"

#include "llvm_config_begin.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Intrinsics.h>
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

TEST_F(CodegenTest, DoNotOptimiseApplyPrimitive)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    DoNotOptimise[I64](0)";

  TEST_COMPILE(src);
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

TEST_F(CodegenTest, RepeatLoopElseClauseJumpsAway)
{
  // From issue #3326
  // The else clause `error` jumps away and so has no type. gen_repeat must not
  // try to reify that absent type, which previously crashed the compiler.
  const char* src =
    "actor Main\n"
      "new create(env: Env) =>\n"
        "try let x: U8 = repeat 1 until false else error end end";

  TEST_COMPILE(src);
}

TEST_F(CodegenTest, LifetimeIntrinsicsHaveSinglePointerArgument)
{
  // LLVM 22 dropped the leading size argument from llvm.lifetime.start and
  // llvm.lifetime.end; they now take a single pointer. A two-argument call
  // is rejected by the module verifier, so confirm codegen emits exactly one
  // argument. The union plus match capture below forces scoped locals, which
  // is what makes codegen emit lifetime markers.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: (U64 | None) = U64(1)\n"
    "    match x\n"
    "    | let n: U64 => None\n"
    "    end";

  TEST_COMPILE(src);

  auto module = llvm::unwrap(compile->module);

  size_t lifetime_calls = 0;
  for(auto& function : *module)
  {
    for(auto& block : function)
    {
      for(auto& inst : block)
      {
        auto intrinsic = llvm::dyn_cast<llvm::IntrinsicInst>(&inst);
        if(intrinsic == nullptr)
          continue;

        auto id = intrinsic->getIntrinsicID();
        if((id == llvm::Intrinsic::lifetime_start) ||
          (id == llvm::Intrinsic::lifetime_end))
        {
          lifetime_calls++;
          ASSERT_EQ(intrinsic->arg_size(), 1u);
        }
      }
    }
  }

  // Guard against a vacuous pass: the program above must actually emit
  // lifetime markers, or the per-call assertion never runs.
  ASSERT_GT(lifetime_calls, 0u);
}
