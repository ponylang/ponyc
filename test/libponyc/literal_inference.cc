#include <gtest/gtest.h>
#include "util.h"

// Literal type inference and limit checks.

#define TEST_ERROR(src) DO(test_error(src, "expr"))
#define TEST_COMPILE(src) DO(test_compile(src, "expr"))


class LiteralTest : public PassTest
{
  protected:
    void check_type_for_assign(const char* type_name, const char* src) {
      TEST_COMPILE(src);

      ast_t* foo_type = type_of("foo");  // Find type of foo
      ast_t* foo = ast_parent(foo_type); // Declaration of foo is parent of type
      ast_t* assign = ast_parent(foo);   // Assignment is the parent of declaration of foo
      ast_t* three = ast_child(assign);  // Literal is the first child of the assignment
      ast_t* type = ast_type(three);     // Finally we get to the type of the literal

      ASSERT_ID(TK_NOMINAL, type);
      ASSERT_STREQ(type_name, ast_name(ast_childidx(type, 1)));  // Child index 1 of a nominal is the type name
  }

};


// First the limitations.

TEST_F(LiteralTest, CantInferLitIs)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    2 is 3";

  // Fails because type of literals cannot be determined. See #369.
  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInferLitIsnt)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    2 isnt 2";

  // Fails because type of literals cannot be determined. See #369.
  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInferLitExpr)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    (1 + 2) is 3";

  // Fails because type of literals cannot be determined. See #369.
  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInferUnionAmbiguousType)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    let x: (U8 | U32) = 17";

  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInferIntersectionType)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    let x: (U8 & U32) = 17\n";  // FIXME 'Multiple possible types for literal' isn't right here. There no types possible.

  TEST_ERROR(src);
}



// Happy paths

TEST_F(LiteralTest, LetSimple)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let foo: U8 = 3";

  DO(check_type_for_assign("U8", src));
}


TEST_F(LiteralTest, LetUnambiguousUnion)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let foo: (U8 | String) = 3";

  DO(check_type_for_assign("U8", src));
}


TEST_F(LiteralTest, LetUnionSameType)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let foo: (U8 | U8) = 3";  // FIXME should union'ing same type together actually be allowed?

  DO(check_type_for_assign("U8", src));
}


