#include <gtest/gtest.h>
#include "util.h"

// Literal type inference and limit checks.

#define TEST_ERROR(src) DO(test_error(src, "expr"))
#define TEST_COMPILE(src) DO(test_compile(src, "expr"))


class LiteralTest : public PassTest
{
  protected:
    void check_type_for_assign(const char* type_name, token_id tk_type, const char* var, const char* src)
    {
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

    void check_type(const char* type_name, token_id tk_type, ast_t* ast) 
    {
      ast_t* type = ast_type(ast);
      ASSERT_ID(tk_type, type);

      switch(tk_type) 
      {
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

TEST_F(LiteralTest, CantInfer_Op_IsLiterals)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    2 is 3";

  // Fails because type of literals cannot be determined. See #369.
  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInfer_Op_IsntLiterals)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    2 isnt 2";

  // Fails because type of literals cannot be determined. See #369.
  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInfer_Op_ExprIsLiteral)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    (1 + 2) is 3";

  // Fails because type of literals cannot be determined. See #369.
  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInfer_Op_IntAndFloat )
{
  const char* src =
    "class Foo10b\n"
    "  fun test(): I32 => 7 and 16.0\n";

  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInfer_Op_FloatAndInt )
{
  const char* src =
    "class Foo11b\n"
    "  fun test(): I32 => 7.0 and 16\n";

  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInfer_Op_FloatAndIntReturningFloat )
{
  const char* src =
    "class Foo12b\n"
    "  fun test(): F32 => 7.0 and 16\n";

  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInfer_Op_IntAndFloatReturningFloat )
{
  const char* src =
    "class Foo13b\n"
    "  fun test(): F32 => 7 and 16.0\n";

  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInfer_Op_EqualsLiterals )
{
  const char* src =
    "class Foo14b\n"
    "  fun test(): Bool => 34 == 79\n";

  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInfer_Let_FloatLiteralFromIntType)
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


TEST_F(LiteralTest, CantInfer_Let_NonNumericType)
{
  DO(check_trivial_fail("String", "3"));
  DO(check_trivial_fail("String", "3.0"));

  DO(check_trivial_fail("Bool", "3"));
  DO(check_trivial_fail("Bool", "3.0"));
}


TEST_F(LiteralTest, CantInfer_Let_NoType)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    let x = 17";

  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInfer_Let_AmbiguousUnion1)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    let x: (U8 | U32) = 17";

  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInfer_Let_AmbiguousUnion2)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    let x: (U8 | F32) = 17";  // could be either U8 or F32

  TEST_ERROR(src);
}

TEST_F(LiteralTest, CantInfer_Let_EmptyIntersection)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    let x: (U8 & U32) = 17\n";

  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInfer_Let_InsufficientlySpecifiedGeneric)
{
  const char* src =
    "class Foo[A]\n"
    "  new create() =>\n"
    "    let x: A = 17";

  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInfer_Let_GenericSubtype)
{
  const char* src =
    " class Foo[A: U8, B: A]\n"
    "  new create() =>\n"
    "    let x: B = 17";   // FIXME feels like this should work - 'let x: A = 17' does

  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInfer_Parameter_GenericSubtype)
{
  const char* src =
    "class Foo6[A : U8, B: A]\n"
    "  fun run() => test(8)\n"
    "  fun test(a: B) => true\n";

  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInfer_Parameter_AmbiguousUnion)
{
  const char* src =
    "class Foo3\n"
    "  fun run() => test(8)\n"
    "  fun test(a: (U8|U32)) => true\n";

  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInfer_Return_InvalidUnion )
{
  const char* src =
    "class Foo4\n"
    "  fun test(): ( Bool | String ) => 2\n";

  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInfer_Array_UnambiguousUnion )
{
  const char* src =
    "class Foo5a\n"
    "  fun run() => test([8])\n"       // FIXME? inferred as Array[I32], not Array[String|I32]
    "  fun test(a: Array[ (String | I32) ] ): Bool => true\n";

  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInfer_Match_LiteralMatchValue )
{
  const char* src =    
    "class Foo10c\n"
    "  fun test(): Bool => \n"
    "    match 8\n"
    "    | I16 => true\n"
    "    end\n";

  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInfer_Match_InvalidElse)
{
  const char* src =    
    "class Foo11c\n"
    "  fun test(q: Bool): I16 => \n"
    "    match q\n"
    "    | true => 1\n"
    "    | false => 1\n"
    "    else\n"
    "      7.0\n"
    "    end\n";

  TEST_ERROR(src);
}


TEST_F(LiteralTest, CantInfer_Literal_Unused)
{
  const char* src =    
    "class Foo12c\n"
    "  fun test(q: Bool): F64 ? => 79; error\n";

  TEST_ERROR(src);
}
    

TEST_F(LiteralTest, CantInfer_While_InvalidReturnValue)
{
  const char* src =
    "class Foo13c\n"
    "  fun test(): String =>\n"
    "    while true do\n"
    "      72\n"
    "    else\n"
    "      ""\n"
    "    end\n";

  TEST_ERROR(src);
}
    

TEST_F(LiteralTest, CantInfer_While_InvalidElseValue )
{
  const char* src =
    "class Foo14c\n"
    "  fun test(): String =>\n"
    "    while true do\n"
    "      ""\n"
    "    else\n"
    "      63\n"
    "    end\n";

  TEST_ERROR(src);
}




// Happy paths

TEST_F(LiteralTest, Let_AllNumericTypes)
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


TEST_F(LiteralTest, Let_UnambiguousUnion)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let x: (U8 | String) = 3";

  TEST_COMPILE(src);

  DO(check_type("U8", TK_NOMINAL, numeric_literal(3)));
}


// TEST_F(LiteralTest, LetUnambiguousUnionBecauseFloatingLiteral)   -- FIXME currently fails
// {
//   const char* src =
//     "class Foo\n"
//     "  fun f() =>\n"
//     "    let x: (U128 | F64) = 36000.0";  // .0 forces this to be F64

//   DO(check_type_for_assign("F64", TK_NOMINAL, "x", src));
// }


TEST_F(LiteralTest, Let_UnionSameType)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let x: (U8 | U8) = 3";

  TEST_COMPILE(src);

  DO(check_type("U8", TK_NOMINAL, numeric_literal(3)));
}


TEST_F(LiteralTest, Let_SufficientlySpecifiedGeneric)
{
  const char* src =
    "class Foo[A: U8]\n"
    "  new create() =>\n"
    "    let x: A = 17";

  TEST_COMPILE(src);

  DO(check_type("A", TK_TYPEPARAMREF, numeric_literal(17)));
}


TEST_F(LiteralTest, Let_Tuple)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    (let x: U8, let y: U32) = (8, 32)";

  TEST_COMPILE(src);

  DO(check_type("U8",  TK_NOMINAL, numeric_literal(8)));
  DO(check_type("U32", TK_NOMINAL, numeric_literal(32)));
}


TEST_F(LiteralTest, Let_NestedTuple)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    ((let w: U8, let x: U16), (let y: U32, let z: U64)) = ((2, 3), (4, 5))";

  TEST_COMPILE(src);

  DO(check_type("U8",  TK_NOMINAL, numeric_literal(2)));
  DO(check_type("U16", TK_NOMINAL, numeric_literal(3)));
  DO(check_type("U32", TK_NOMINAL, numeric_literal(4)));
  DO(check_type("U64", TK_NOMINAL, numeric_literal(5)));
}


TEST_F(LiteralTest, Let_TupleOfGeneric)
{
  const char* src =
    "class Foo[A: U8, B: U32]\n"
    "  new create() =>\n"
    "    (let x: A, let y: A) = (16, 32)";
  
  TEST_COMPILE(src);
  
  DO(check_type("A", TK_TYPEPARAMREF, numeric_literal(16)));
  DO(check_type("A", TK_TYPEPARAMREF, numeric_literal(32)));
}


TEST_F(LiteralTest, Let_TupleOfGenericOfGeneric)
{
  const char* src =
    "class Foo[A: U8, B: U32]\n"
    "  new create() =>\n"
    "    (let x: A, let y: B) = (16, 32)";

  TEST_COMPILE(src);
  
  DO(check_type("A", TK_TYPEPARAMREF, numeric_literal(16)));
  DO(check_type("B", TK_TYPEPARAMREF, numeric_literal(32)));
}


TEST_F(LiteralTest, Parameter_Simple)
{
  const char* src =
    "class Foo1\n"
    "  fun run() => test(128)\n"
    "  fun test(a: U128) => true\n";

  TEST_COMPILE(src);

  DO(check_type("U128", TK_NOMINAL, numeric_literal(128)));
}


TEST_F(LiteralTest, Parameter_SecondParameter)
{
  const char* src =
    "class Foo1a\n"
    "  fun run() => test(\"\", 128)\n"
    "  fun test(a: String, b: U128) => true\n";

  TEST_COMPILE(src);

  DO(check_type("U128",  TK_NOMINAL, numeric_literal(128)));
}


TEST_F(LiteralTest, Parameter_Tuple)
{
  const char* src =
    "class Foo2\n"
    "  fun run() => test((8, 32))\n"
    "  fun test(a: (U8, U32)) => true\n";

  TEST_COMPILE(src);

  DO(check_type("U8",  TK_NOMINAL, numeric_literal(8)));
  DO(check_type("U32", TK_NOMINAL, numeric_literal(32)));
}


TEST_F(LiteralTest, Parameter_NestedTuple)
{
  const char* src =
    "class Foo2a\n"
    "  fun run() => test((8, (\"\", 32)))\n"
    "  fun test(a: (U8, (String, U32)) ) => true\n";

  TEST_COMPILE(src);

  DO(check_type("U8",  TK_NOMINAL, numeric_literal(8)));
  DO(check_type("U32", TK_NOMINAL, numeric_literal(32)));
}


TEST_F(LiteralTest, Parameter_Union)
{
  const char* src =
    "class Foo4\n"
    "  fun run() => test(64)\n"
    "  fun test(a: (Bool | U64 | String) ) => true\n";

  TEST_COMPILE(src);

  DO(check_type("U64",  TK_NOMINAL, numeric_literal(64)));
}


TEST_F(LiteralTest, Parameter_TupleOfUnions)
{
  const char* src =
    "class Foo7\n"
    "  fun run() => test((8, 32))\n"
    "  fun test(a: ((U8 | String), (Bool | U32)) ) => true\n";

  TEST_COMPILE(src);

  DO(check_type("U8",  TK_NOMINAL, numeric_literal(8)));
  DO(check_type("U32", TK_NOMINAL, numeric_literal(32)));
}


TEST_F(LiteralTest, Parameter_Generic)
{
  const char* src =
    "class Foo5[A : U8]\n"
    "  fun run() => test(8)\n"
    "  fun test(a: A) => true\n";

  TEST_COMPILE(src);

  DO(check_type("A",  TK_TYPEPARAMREF, numeric_literal(8)));
}


TEST_F(LiteralTest, Parameter_TupleOfUnionOfGeneric)
{
  const char* src =
    "class Foo7a[A : U32]\n"
    "  fun run() => test((79, 1032))\n"
    "  fun test(a: (U8, (String | A)) ) => true\n";

  TEST_COMPILE(src);

  DO(check_type("U8", TK_NOMINAL, numeric_literal(79)));
  DO(check_type("A",  TK_TYPEPARAMREF, numeric_literal(1032)));
}


TEST_F(LiteralTest, Return_Simple)
{
  const char* src =
    "class Foo1\n"
    "  fun test(): U128 => 17\n";

  TEST_COMPILE(src);

  DO(check_type("U128", TK_NOMINAL, numeric_literal(17)));
}


TEST_F(LiteralTest, Return_TupleOfUnionOfGeneric)
{
  const char* src =
    "class Foo2[A]\n"
    "  fun test(): (String, (U8|A)) => (\"\", 2)\n";

  TEST_COMPILE(src);

  DO(check_type("U8", TK_NOMINAL, numeric_literal(2)));
}


TEST_F(LiteralTest, Return_UnionOfTuple)
{
  const char* src =
    "class Foo3\n"
    "  fun test(): ( (String, U32) | (U16, String) ) => (\"\", 2)\n";

  TEST_COMPILE(src);

  DO(check_type("U32", TK_NOMINAL, numeric_literal(2)));
}


TEST_F(LiteralTest, UseReturn_Simple)
{
  const char* src =
    "class Foo10\n"
    "  fun run() => 8 * test()\n"
    "  fun test(): U8 => 2\n";

  TEST_COMPILE(src);

  DO(check_type("U8", TK_NOMINAL, numeric_literal(8)));
}


TEST_F(LiteralTest, UseReturn_Tuple)
{
  const char* src =
    "class Foo11\n"
    "  fun run() => 8 * test()._2\n"
    "  fun test(): (String, I16) => (\"\", 16)\n";

  TEST_COMPILE(src);

  DO(check_type("I16", TK_NOMINAL, numeric_literal(8)));
}


TEST_F(LiteralTest, Type_Simple)
{
  const char* src =
    "type T is ( (String, U32) | (U16, String) )\n"
    "\n"
    "class Foo1t\n"
    "  fun test(): T => (\"\", 2)\n";

  TEST_COMPILE(src);

  DO(check_type("U32", TK_NOMINAL, numeric_literal(2)));
}


TEST_F(LiteralTest, Array_Simple)
{
  const char* src =
    "class Foo1a\n"
    "  fun test(): Array[U8] => [8]\n";

  TEST_COMPILE(src);

  DO(check_type("U8", TK_NOMINAL, numeric_literal(8)));
}


TEST_F(LiteralTest, Array_Parameter)
{
  const char* src =
    "class Foo2a\n"
    "  fun run() => test([8])\n"
    "  fun test(a: Array[I32]): Bool => true\n";

  TEST_COMPILE(src);

  DO(check_type("I32", TK_NOMINAL, numeric_literal(8)));
}


TEST_F(LiteralTest, Array_ParameterOfUnionOfArray)
{
  const char* src =
    "class Foo3a\n"
    "  fun run() => test([8])\n"
    "  fun test(a: (Array[String] | Array[I32]) ): Bool => true\n";

  TEST_COMPILE(src);

  DO(check_type("I32", TK_NOMINAL, numeric_literal(8)));
}


TEST_F(LiteralTest, Array_ParameterOfArrayOfUnion)
{
  const char* src =
    "class Foo4a\n"
    "  fun run() => test([8, \"\"])\n"
    "  fun test(a: Array[ (String | I32) ] ): Bool => true\n";

  TEST_COMPILE(src);

  DO(check_type("I32", TK_NOMINAL, numeric_literal(8)));
}


TEST_F(LiteralTest, Op_MulLiterals)
{
  const char* src =
    "class Foo1b\n"
    "  fun test(): I64 => 64 * 79\n";

  TEST_COMPILE(src);

  DO(check_type("I64", TK_NOMINAL, numeric_literal(64)));
  DO(check_type("I64", TK_NOMINAL, numeric_literal(79)));
}


TEST_F(LiteralTest, Op_XorLiterals)
{
  const char* src =
    "class Foo2b\n"
    "  fun test(): I64 => 64 xor 79\n";

  TEST_COMPILE(src);

  DO(check_type("I64", TK_NOMINAL, numeric_literal(64)));
  DO(check_type("I64", TK_NOMINAL, numeric_literal(79)));
}


TEST_F(LiteralTest, Op_VarOrLiteral)
{
  const char* src =
    "class Foo2ba\n"
    "  fun test(y: I64): I64 => y or 79\n";

  TEST_COMPILE(src);

  DO(check_type("I64", TK_NOMINAL, numeric_literal(79)));
}


TEST_F(LiteralTest, Op_VarEqualsLiteral)
{
  const char* src =
    "class Foo4b\n"
    "  fun test(y: I32): Bool => y == 79\n";

  TEST_COMPILE(src);

  DO(check_type("I32", TK_NOMINAL, numeric_literal(79)));
}


TEST_F(LiteralTest, Op_LiteralEqualsVar)
{
  const char* src =
    "class Foo5b\n"
    "  fun test(y: I32): Bool => 79 == y\n";

  TEST_COMPILE(src);

  DO(check_type("I32", TK_NOMINAL, numeric_literal(79)));
}


TEST_F(LiteralTest, Op_ParameterOfTupleOfAnd)
{
  const char* src =
    "class Foo3b\n"
    "  fun run() => test((\"\", 64 and 79))\n"
    "  fun test(a: (String, I64)) : Bool => true\n";

  TEST_COMPILE(src);

  DO(check_type("I64", TK_NOMINAL, numeric_literal(64)));
  DO(check_type("I64", TK_NOMINAL, numeric_literal(79)));
}


TEST_F(LiteralTest, If_Simple)
{
  const char* src =
   "class Foo1c\n"
    "  fun test(): USize => if true then 79 elseif false then 234 else 123 end\n";

  TEST_COMPILE(src);

  DO(check_type("USize", TK_NOMINAL, numeric_literal(79)));
  DO(check_type("USize", TK_NOMINAL, numeric_literal(234)));
  DO(check_type("USize", TK_NOMINAL, numeric_literal(123)));
}


TEST_F(LiteralTest, Match_ResultInt)
{
  const char* src =
    "class Foo2c\n"
    "  fun test(q: Bool): I16 => \n"
    "    match q\n"
    "    | true => 1\n"
    "    | false => 11\n"
    "    else\n"
    "      7\n"
    "    end\n";

  TEST_COMPILE(src);

  DO(check_type("I16", TK_NOMINAL, numeric_literal(1)));
  DO(check_type("I16", TK_NOMINAL, numeric_literal(11)));
  DO(check_type("I16", TK_NOMINAL, numeric_literal(7)));
}


TEST_F(LiteralTest, Match_ResultFloat)
{
  const char* src =
    "class Foo3c\n"
    "  fun test(q: Bool): F64 => \n"
    "    match q\n"
    "    | true => 1\n"
    "    | false => 11\n"
    "    else\n"
    "      7\n"
    "    end\n";

  TEST_COMPILE(src);

  DO(check_type("F64", TK_NOMINAL, numeric_literal(1)));
  DO(check_type("F64", TK_NOMINAL, numeric_literal(11)));
  DO(check_type("F64", TK_NOMINAL, numeric_literal(7)));
}


TEST_F(LiteralTest, Match_PatternTuple)
{
  const char* src =
    "class Foo4c\n"
    "  fun test(q: (U16, F32)): Bool => \n"
    "    match q\n"
    "    | (16, 17) => true\n"
    "    | (32, 33) => false\n"
    "    else\n"
    "      false\n"
    "    end\n";

  TEST_COMPILE(src);

  DO(check_type("U16", TK_NOMINAL, numeric_literal(16)));
  DO(check_type("U16", TK_NOMINAL, numeric_literal(32)));
  DO(check_type("F32", TK_NOMINAL, numeric_literal(17)));
  DO(check_type("F32", TK_NOMINAL, numeric_literal(33)));
}


TEST_F(LiteralTest, Try_ErrorBodyElseLiteral)
{
  const char* src =
    "class Foo5c\n"
    "  fun test(): F64 => try error else 73 end\n";

  TEST_COMPILE(src);

  DO(check_type("F64", TK_NOMINAL, numeric_literal(73)));
}


TEST_F(LiteralTest, Try_ExpressionBody)
{
  const char* src =
    "class Foo6c\n"
    "  fun run() => test(true)\n"
    "  fun test(q: Bool): F64 => try if q then 166 else error end else 73 end\n";

  TEST_COMPILE(src);

  DO(check_type("F64", TK_NOMINAL, numeric_literal(166)));
  DO(check_type("F64", TK_NOMINAL, numeric_literal(73)));
}


TEST_F(LiteralTest, While_TupleResultBreakHasValue)
{
  const char* src =
    "class Foo7c\n"
    "  fun test(): (U128, Bool) =>\n"
    "    while true do\n"
    "      if false then\n"
    "        (72 * 16, true)\n"
    "      else\n"
    "        break (14, true)\n"
    "      end\n"
    "    else\n"
    "      (63 / 12, false)\n"
    "    end\n";

  TEST_COMPILE(src);

  DO(check_type("U128", TK_NOMINAL, numeric_literal(72)));
  DO(check_type("U128", TK_NOMINAL, numeric_literal(16)));
  DO(check_type("U128", TK_NOMINAL, numeric_literal(14)));
  DO(check_type("U128", TK_NOMINAL, numeric_literal(63)));
  DO(check_type("U128", TK_NOMINAL, numeric_literal(12)));
}


TEST_F(LiteralTest, While_TupleResultBreakHasNoValue)
{
  const char* src =
    "class Foo8c\n"
    "  fun test(): (None | (U128, Bool)) =>\n"
    "    while true do\n"
    "      if false then\n"
    "        (72 * 16, true)\n"
    "      else\n"
    "        break\n"
    "      end\n"
    "    else\n"
    "      (63 / 12, false)\n"
    "    end\n";

  TEST_COMPILE(src);

  DO(check_type("U128", TK_NOMINAL, numeric_literal(72)));
  DO(check_type("U128", TK_NOMINAL, numeric_literal(16)));
  DO(check_type("U128", TK_NOMINAL, numeric_literal(63)));
  DO(check_type("U128", TK_NOMINAL, numeric_literal(12)));
}


TEST_F(LiteralTest, For_Simple)
{
  const char* src =
    "class Foo9c\n"
    "  fun test(q: Array[String], r: Bool): U64 =>\n"
    "    for x in q.values() do\n"
    "      if r then 3 else 4 end\n"
    "    else\n"
    "      0\n"
    "    end\n";

  TEST_COMPILE(src);

  DO(check_type("U64", TK_NOMINAL, numeric_literal(3)));
  DO(check_type("U64", TK_NOMINAL, numeric_literal(4)));
  DO(check_type("U64", TK_NOMINAL, numeric_literal(0)));
}
