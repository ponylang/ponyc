#include <gtest/gtest.h>
#include <platform.h>

#include <reach/reach.h>
#include <type/subtype.h>

#include "util.h"

#include <memory>


#define TEST_COMPILE(src, pass) DO(test_compile(src, pass))


class ReachTest : public PassTest
{};


TEST_F(ReachTest, IsectHasSubtypes)
{
  const char* src =
    "trait TA\n"
    "  fun a()\n"

    "trait TB\n"
    "  fun b()\n"

    "actor Main is (TA & TB)\n"
    "  new create(env: Env) =>\n"
    "    let ab: (TA & TB) = this\n"
    "    ab.a()\n"
    "    ab.b()\n"

    "  fun a() => None\n"
    "  fun b() => None";

  TEST_COMPILE(src, "reach");

  ast_t* ab_ast = type_of("ab");
  ASSERT_NE(ab_ast, (void*)NULL);

  reach_t* reach = compile->reach;
  reach_type_t* ab_reach = reach_type(reach, ab_ast);
  ASSERT_NE(ab_reach, (void*)NULL);

  size_t i = HASHMAP_BEGIN;
  reach_type_t* subtype;

  bool found = false;
  while((subtype = reach_type_cache_next(&ab_reach->subtypes, &i)) != NULL)
  {
    if(subtype->name == stringtab("Main"))
    {
      found = true;
      break;
    }
  }

  ASSERT_TRUE(found);
}

struct reach_deleter
{
  void operator()(reach_t* r)
  {
    reach_free(r);
  }
};

TEST_F(ReachTest, Determinism)
{
  const char* src =
    "interface I1\n"
    "  fun f1()\n"
    "interface I2\n"
    "  fun f2()\n"
    "interface I3\n"
    "  fun f3()\n"
    "interface I4\n"
    "  fun f4()\n"

    "class C1 is (I1 & I2)\n"
    "  fun f1() => None\n"
    "  fun f2() => None\n"
    "class C2 is (I2 & I3)\n"
    "  fun f2() => None\n"
    "  fun f3() => None\n"
    "class C3 is (I3 & I4)\n"
    "  fun f3() => None\n"
    "  fun f4() => None\n"
    "class C4 is (I4 & I1)\n"
    "  fun f4() => None\n"
    "  fun f1() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    env.out.print(\"\")\n"

    "    U8(0) ; I8(0) ; U16(0) ; I16(0) ; U32(0) ; I32(0)\n"

    "    (C1, C2)\n"
    "    (C2, C3)\n"
    "    (C3, C4)\n"
    "    (C4, C1)\n"

    "    let i1: I1 = C1\n"
    "    let i2: I2 = C2\n"
    "    let i3: I3 = C3\n"
    "    let i4: I4 = C4\n"

    "    i1.f1()\n"
    "    i2.f2()\n"
    "    i3.f3()\n"
    "    i4.f4()";

  set_builtin(nullptr);

  TEST_COMPILE(src, "paint");

  std::unique_ptr<reach_t, reach_deleter> reach_guard{compile->reach};
  compile->reach = nullptr;

  TEST_COMPILE(src, "paint");

  reach_t* r1 = reach_guard.get();
  reach_t* r2 = compile->reach;

  ASSERT_EQ(r1->object_type_count, r2->object_type_count);
  ASSERT_EQ(r1->numeric_type_count, r2->numeric_type_count);
  ASSERT_EQ(r1->tuple_type_count, r2->tuple_type_count);
  ASSERT_EQ(r1->total_type_count, r2->total_type_count);
  ASSERT_EQ(r1->trait_type_count, r2->trait_type_count);

  reach_type_t* t1;
  size_t i = HASHMAP_BEGIN;

  while((t1 = reach_types_next(&r1->types, &i)) != nullptr)
  {
    size_t j = HASHMAP_UNKNOWN;
    reach_type_t* t2 = reach_types_get(&r2->types, t1, &j);

    ASSERT_NE(t2, nullptr);
    ASSERT_EQ(t1->type_id, t2->type_id);

    reach_method_name_t* n1;
    j = HASHMAP_BEGIN;

    while((n1 = reach_method_names_next(&t1->methods, &j)) != nullptr)
    {
      size_t k = HASHMAP_UNKNOWN;
      reach_method_name_t* n2 = reach_method_names_get(&t2->methods, n1, &j);

      ASSERT_NE(n2, nullptr);

      reach_method_t* m1;
      k = HASHMAP_BEGIN;

      while((m1 = reach_mangled_next(&n1->r_mangled, &k)) != nullptr)
      {
        size_t l = HASHMAP_UNKNOWN;
        reach_method_t* m2 = reach_mangled_get(&n2->r_mangled, m1, &l);

        ASSERT_NE(m2, nullptr);
        ASSERT_EQ(m1->vtable_index, m2->vtable_index);
      }
    }
  }
}
