#include <gtest/gtest.h>
#include "util.h"

// Literal type inference and limit checks.

#define TEST_ERROR(src) DO(test_error(src, "expr"))
#define TEST_COMPILE(src) DO(test_compile(src, "expr"))


class LiteralTest : public PassTest
{
  protected:
    void check_type_for_assign(const char* type_name, token_id tk_type, const char* var, const char* src) {
      TEST_COMPILE(src);

      ast_t* x_type = type_of(var);      // Find type of x
      ast_t* x = ast_parent(x_type);     // Declaration of foo is parent of type
      ast_t* assign = ast_parent(x);     // Assignment is the parent of declaration of x
      ast_t* three = ast_child(assign);  // Literal is the first child of the assignment
      ast_t* type = ast_type(three);     // Finally we get to the type of the literal

      ASSERT_ID(tk_type, type);

      switch(tk_type) {
        case TK_NOMINAL:
          ASSERT_STREQ(type_name, ast_name(ast_childidx(type, 1)));  // Child index 1 of a nominal is the type name
          break;
        case TK_TYPEPARAMREF:
          ASSERT_STREQ(type_name, ast_name(ast_childidx(type, 0)));  // Child index 0 of a type param is the type name
          break;
        default:
          FAIL(); // Unexpected tk_type
      }
    }

    void check_trivial(const char* type_name, const char* value) {
      const char* src =
        "class Foo\n"
        "  fun f() =>\n"
        "    let x: ";

      std::string str(src);
      str.append(type_name).append(" = ").append(value);

      DO(check_type_for_assign(type_name, TK_NOMINAL, "x", str.c_str()));
    }


    void check_trivial_fail(const char* type_name, const char* value) {
      const char* src =
        "class Foo\n"
        "  fun f() =>\n"
        "    let x: ";

      std::string str(src);
      str.append(type_name).append(" = ").append(value);

      TEST_ERROR(str.c_str());
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


TEST_F(LiteralTest, CantInferFloatLiteralFromIntType)
{
  DO(check_trivial_fail("U8",   "3.0"));
  DO(check_trivial_fail("U16",  "3.0"));
  DO(check_trivial_fail("U32",  "3.0"));
  DO(check_trivial_fail("U64",  "3.0"));
  DO(check_trivial_fail("U128", "3.0"));

  DO(check_trivial_fail("I8",   "3.0"));
  DO(check_trivial_fail("I16",  "3.0"));
  DO(check_trivial_fail("I32",  "3.0"));
  DO(check_trivial_fail("I64",  "3.0"));
  DO(check_trivial_fail("I128", "3.0"));
}


TEST_F(LiteralTest, CantInferNumericLiteralFromNonNumericType)
{
  DO(check_trivial_fail("String", "3"));
  DO(check_trivial_fail("String", "3.0"));

  DO(check_trivial_fail("Bool", "3"));
  DO(check_trivial_fail("Bool", "3.0"));
}


TEST_F(LiteralTest, CantInferNumericTypesWithoutVarType)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    let x = 17";

  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInferAmbiguousUnion)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    let x: (U8 | U32) = 17";

  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInferIntegerLiteralWithAmbiguousUnion)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    let x: (U8 | F32) = 17";  // could be either U8 or F32

  TEST_ERROR(src);
}

TEST_F(LiteralTest, CantInferEmptyIntersection)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    let x: (U8 & U32) = 17\n";

  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInferInsufficientlySpecifiedGeneric)
{
  const char* src =
    "class Foo[A]\n"
    "  new create() =>\n"
    "    let x: A = 17";

  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInferThroughGenericSubtype)
{
  const char* src =
    " class Foo[A: U8, B: A]\n"
    "  new create() =>\n"
    "    let x: B = 17";   // FIXME feels like this should work - 'let x: A = 17' does

  TEST_ERROR(src);
}



// Happy paths

TEST_F(LiteralTest, TrivialLetAllNumericTypes)
{
  DO(check_trivial("U8",   "3"));
  DO(check_trivial("U16",  "3"));
  DO(check_trivial("U32",  "3"));
  DO(check_trivial("U64",  "3"));
  DO(check_trivial("U128", "3"));

  DO(check_trivial("I8",   "3"));
  DO(check_trivial("I16",  "3"));
  DO(check_trivial("I32",  "3"));
  DO(check_trivial("I64",  "3"));
  DO(check_trivial("I128", "3"));

  DO(check_trivial("F32",  "3"));
  DO(check_trivial("F64",  "3"));

  DO(check_trivial("F32",  "3.0"));
  DO(check_trivial("F64",  "3.0"));
}


TEST_F(LiteralTest, LetUnambiguousUnion)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let x: (U8 | String) = 3";

  DO(check_type_for_assign("U8", TK_NOMINAL, "x", src));
}


// TEST_F(LiteralTest, LetUnambiguousUnionBecauseFloatingLiteral)   -- FIXME currently fails
// {
//   const char* src =
//     "class Foo\n"
//     "  fun f() =>\n"
//     "    let x: (U128 | F64) = 36000.0";  // .0 forces this to be F64

//   DO(check_type_for_assign("F64", TK_NOMINAL, "x", src));
// }


TEST_F(LiteralTest, LetUnionSameType)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let x: (U8 | U8) = 3";

  DO(check_type_for_assign("U8", TK_NOMINAL, "x", src));
}


TEST_F(LiteralTest, LetSufficientlySpecifiedGeneric)
{
  const char* src =
    "class Foo[A: U8]\n"
    "  new create() =>\n"
    "    let x: A = 17";

  DO(check_type_for_assign("A", TK_TYPEPARAMREF, "x", src));
}


TEST_F(LiteralTest, LetTuple)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    (let x: U8, let y: U32) = (8, 32)";

  DO(check_type_for_assign("U8",  TK_NOMINAL, "x", src));
  DO(check_type_for_assign("U32", TK_NOMINAL, "y", src));
}


TEST_F(LiteralTest, LetTupleNested)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    ((let w: U8, let x: U16), (let y: U32, let z: U64)) = ((2, 3), (4, 5))";

  DO(check_type_for_assign("U8",  TK_NOMINAL, "w", src));
  DO(check_type_for_assign("U16", TK_NOMINAL, "x", src));
  DO(check_type_for_assign("U32", TK_NOMINAL, "y", src));
  DO(check_type_for_assign("U64", TK_NOMINAL, "z", src));
}


TEST_F(LiteralTest, LetTupleOfGeneric_FirstOnly)
{
  const char* src =
    "class Foo[A: U8, B: U32]\n"
    "  new create() =>\n"
    "    (let x: A, let y: A) = (16, 32)";

  DO(check_type_for_assign("A", TK_TYPEPARAMREF, "x", src));
  DO(check_type_for_assign("A", TK_TYPEPARAMREF, "y", src));
}

// TEST_F(LiteralTest, LetTupleOfGeneric_FirstAndSecond)   // FIXME only diff here is 'let y: B', but it doesn't work
// {
//   const char* src =
//     "class Foo[A: U8, B: U32]\n"
//     "  new create() =>\n"
//     "    (let x: A, let y: B) = (16, 32)";

//   DO(check_type_for_assign("A", TK_TYPEPARAMREF, "x", src));
//   DO(check_type_for_assign("B", TK_TYPEPARAMREF, "y", src));
// }


