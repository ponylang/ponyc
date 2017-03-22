#include <gtest/gtest.h>
#include <platform.h>

#include <reach/reach.h>
#include <type/subtype.h>

#include "util.h"


#define TEST_COMPILE(src) DO(test_compile(src, "reach"))


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

  TEST_COMPILE(src);

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
