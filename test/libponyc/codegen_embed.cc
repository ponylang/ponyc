#include <gtest/gtest.h>
#include <platform.h>
#include <codegen/gentype.h>
#include <reach/reach.h>
#include "util.h"
#include <llvm-c/Core.h>
#include "llvm_config_begin.h"
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Module.h>
#include "llvm_config_end.h"

#define TEST_COMPILE(src) DO(test_compile(src, "ir"))

// Traces of a type that can share memory with other objects go to the
// runtime's embeddable entry points; every other trace goes to the plain ones.

class CodegenEmbedTest : public PassTest
{
protected:
  // Calls to the named function, in functions whose name starts with prefix.
  size_t calls_to(const char* name, const char* prefix = "")
  {
    auto module = llvm::unwrap(compile->module);
    size_t count = 0;

    for(auto& function : *module)
    {
      if(!function.getName().starts_with(prefix))
        continue;

      for(auto& block : function)
      {
        for(auto& inst : block)
        {
          auto call = llvm::dyn_cast<llvm::CallBase>(&inst);
          if(call == nullptr)
            continue;

          auto callee = call->getCalledFunction();
          if((callee != nullptr) && (callee->getName() == name))
            count++;
        }
      }
    }

    return count;
  }

  uint8_t flags_of(const char* type_name)
  {
    reach_type_t* t = reach_type_name(compile->reach, type_name, &opt);
    if(t == NULL)
      return 0xFF;

    return ((compile_type_t*)t->c_type)->embed_flags;
  }
};

TEST_F(CodegenEmbedTest, ProgramWithoutEmbedUsesPlainTraces)
{
  const char* src =
    "interface Thing\n"
    "  fun value(): U64\n"

    "class A is Thing\n"
    "  let s: String = \"a\"\n"
    "  fun value(): U64 => 1\n"

    "actor Keeper\n"
    "  var _t: (Thing | None) = None\n"
    "  be take(t: Thing iso) => _t = consume t\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Keeper.take(recover A end)";

  TEST_COMPILE(src);

  ASSERT_GT(calls_to("pony_traceknown") + calls_to("pony_traceunknown"), 0u);
  ASSERT_EQ(0u, calls_to("pony_traceknown_embeddable"));
  ASSERT_EQ(0u, calls_to("pony_traceunknown_embeddable"));
}

// A union of a tuple and an embeddable class: the trace that checks for a
// boxed tuple can also see the class, so it must be routed.
TEST_F(CodegenEmbedTest, UnionWithTupleAndEmbeddableTypeIsRouted)
{
  const char* src =
    "class C\n"
    "  var x: U64 = 7\n"

    "class A\n"
    "  embed c: C = C\n"

    "actor Keeper\n"
    "  var _v: ((String, String) | C val) = (\"a\", \"b\")\n"
    "  be take(v: ((String, String) | C val)) => _v = v\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a: A val = recover A end\n"
    "    Keeper.take(a.c)";

  TEST_COMPILE(src);

  // Keeper traces the union when it receives it and when it collects.
  ASSERT_GT(calls_to("pony_traceunknown_embeddable", "Keeper_"), 0u);
  ASSERT_EQ(0u, calls_to("pony_traceunknown", "Keeper_"));
}

// A field with no size sits at the same offset as the field after it, so both
// share the struct's address; the one with a size starts the struct.
TEST_F(CodegenEmbedTest, ZeroSizeFieldBeforeTheFirstRealField)
{
  const char* src =
    "struct Z\n"
    "  new create() => None\n"

    "class T\n"
    "  var x: U64 = 0\n"

    "struct S\n"
    "  embed z: Z = Z\n"
    "  embed t: T = T\n"
    "  var f: U64 = 0\n"
    "  new create() => None\n"

    "actor Main\n"
    "  var _s: S = S\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src);

  reach_type_t* s = reach_type_name(compile->reach, "S", &opt);
  reach_type_t* t = reach_type_name(compile->reach, "T", &opt);
  ASSERT_NE(nullptr, s);
  ASSERT_NE(nullptr, t);
  ASSERT_EQ(t, gentype_first_embed(compile, s));

  ASSERT_NE(0, flags_of("S") & PONY_EMBED_ROUTED);
  ASSERT_NE(0, flags_of("T") & PONY_EMBED_AT_START);
  ASSERT_NE(0, flags_of("Z") & PONY_EMBED_AT_START);
}

// Each reification is its own type: embedding one doesn't route another.
TEST_F(CodegenEmbedTest, OnlyTheEmbeddedReificationIsRouted)
{
  const char* src =
    "class H\n"
    "  embed a: Array[U8] = Array[U8]\n"

    "actor Main\n"
    "  var _h: H = H\n"
    "  var _b: Array[U16] = Array[U16]\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src);

  ASSERT_NE(0, flags_of("Array_U8_val") & PONY_EMBED_ROUTED);
  ASSERT_EQ(0, flags_of("Array_U16_val") & PONY_EMBED_ROUTED);
}

// A type embedded in an actor can be found inside one, and the actor must be
// findable from the address of that field.
TEST_F(CodegenEmbedTest, EmbeddedInAnActor)
{
  const char* src =
    "class C\n"
    "  var x: U64 = 0\n"

    "class D\n"
    "  var y: U64 = 0\n"

    "class H\n"
    "  embed d: D = D\n"

    "actor Main\n"
    "  embed c: C = C\n"
    "  var _h: H = H\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src);

  ASSERT_NE(0, flags_of("C") & PONY_EMBED_IN_ACTOR);
  ASSERT_NE(0, flags_of("Main") & PONY_EMBED_ACTOR_LOOKUP);
  ASSERT_EQ(0, flags_of("D") & PONY_EMBED_IN_ACTOR);
  ASSERT_EQ(0, flags_of("H") & PONY_EMBED_ACTOR_LOOKUP);
}
