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
// runtime's routed entry points; every other trace goes to the plain ones.

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
    {
      ADD_FAILURE() << "no reachable type " << type_name;
      return 0;
    }

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
  ASSERT_EQ(0u, calls_to("pony_traceknown_routed"));
  ASSERT_EQ(0u, calls_to("pony_traceunknown_routed"));
}

// A union of a tuple and a routed class: the trace that checks for a
// boxed tuple can also receive a C, so it must be routed.
TEST_F(CodegenEmbedTest, UnionWithTupleAndEmbeddedTypeIsRouted)
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
  ASSERT_GT(calls_to("pony_traceunknown_routed", "Keeper_"), 0u);
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

    "class F\n"
    "  var z: U64 = 0\n"

    "class E\n"
    "  embed f: F = F\n"

    "actor Main\n"
    "  embed c: C = C\n"
    "  embed e: E = E\n"
    "  var _h: H = H\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src);

  ASSERT_NE(0, flags_of("C") & PONY_EMBED_IN_ACTOR);
  // Embedded in an object that is embedded in an actor.
  ASSERT_NE(0, flags_of("F") & PONY_EMBED_IN_ACTOR);
  ASSERT_NE(0, flags_of("Main") & PONY_EMBED_ACTOR_LOOKUP);
  ASSERT_EQ(0, flags_of("D") & PONY_EMBED_IN_ACTOR);
  ASSERT_EQ(0, flags_of("H") & PONY_EMBED_ACTOR_LOOKUP);
}

// A trace through an interface or trait reaches every class that provides it,
// so it is routed when one of them is embedded somewhere.
TEST_F(CodegenEmbedTest, InterfaceAndTraitReachingAnEmbeddedClassAreRouted)
{
  const char* src =
    "interface I\n"
    "  fun i(): U64\n"

    "trait T\n"
    "  fun t(): U64\n"

    "class C is (I & T)\n"
    "  var x: U64 = 7\n"
    "  fun i(): U64 => x\n"
    "  fun t(): U64 => x\n"

    "class A\n"
    "  embed c: C = C\n"

    "actor Keeper\n"
    "  var _i: (I val | None) = None\n"
    "  var _t: (T val | None) = None\n"
    "  be take_i(i: I val) => _i = i\n"
    "  be take_t(t: T val) => _t = t\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a: A val = recover A end\n"
    "    let k = Keeper\n"
    "    k.take_i(a.c)\n"
    "    k.take_t(a.c)";

  TEST_COMPILE(src);

  ASSERT_GT(calls_to("pony_traceunknown_routed", "Keeper_"), 0u);
  ASSERT_EQ(0u, calls_to("pony_traceunknown", "Keeper_"));
}

// A struct whose first field with a size isn't embedded doesn't share its
// address with an embedded object.
TEST_F(CodegenEmbedTest, StructStartingWithAPlainFieldIsNotRouted)
{
  const char* src =
    "class C\n"
    "  var x: U64 = 0\n"

    "struct S\n"
    "  var a: U64 = 0\n"
    "  embed c: C = C\n"
    "  new create() => None\n"

    "actor Main\n"
    "  var _s: S = S\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src);

  reach_type_t* s = reach_type_name(compile->reach, "S", &opt);
  ASSERT_NE(nullptr, s);
  ASSERT_EQ(nullptr, gentype_first_embed(compile, s));
  ASSERT_EQ(0, flags_of("S") & PONY_EMBED_ROUTED);
}

// A struct that starts with an embedded field is allocated so the GC knows
// its start is shared; a struct that starts with a plain field isn't.
TEST_F(CodegenEmbedTest, StructSharingItsStartIsAllocatedAsSuch)
{
  const char* src =
    "class E\n"
    "  var x: U64 = 0\n"

    "struct Shared\n"
    "  embed e: E = E\n"
    "  var y: U64 = 0\n"
    "  new create() => None\n"

    "struct Plain\n"
    "  var y: U64 = 0\n"
    "  embed e: E = E\n"
    "  new create() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    make_shared()\n"
    "    make_plain()\n"
    "  fun make_shared(): Shared => Shared\n"
    "  fun make_plain(): Plain => Plain";

  TEST_COMPILE(src);

  ASSERT_GT(calls_to("pony_alloc_small_shared_start", "Main_ref_make_shared"), 0u);
  ASSERT_EQ(0u, calls_to("pony_alloc_small_shared_start", "Main_ref_make_plain"));
  ASSERT_GT(calls_to("pony_alloc_small", "Main_ref_make_plain"), 0u);
}

// A trace of a field whose type is embedded somewhere is routed.
TEST_F(CodegenEmbedTest, KnownTraceOfAnEmbeddedTypeIsRouted)
{
  const char* src =
    "class C\n"
    "  var x: U64 = 7\n"

    "class A\n"
    "  embed c: C = C\n"

    "actor Keeper\n"
    "  var _c: C val = recover C end\n"
    "  be take(c: C val) => _c = c\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a: A val = recover A end\n"
    "    Keeper.take(a.c)";

  TEST_COMPILE(src);

  ASSERT_GT(calls_to("pony_traceknown_routed", "Keeper_"), 0u);
}
