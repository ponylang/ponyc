#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"


#define TEST_COMPILE(src) DO(test_compile(src, "refer"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "refer", errs)); }

#define TEST_ERRORS_2(src, err1, err2) \
  { const char* errs[] = {err1, err2, NULL}; \
    DO(test_expected_errors(src, "refer", errs)); }

#define TEST_ERRORS_3(src, err1, err2, err3) \
  { const char* errs[] = {err1, err2, err3, NULL}; \
    DO(test_expected_errors(src, "refer", errs)); }

#define TEST_ERRORS_4(src, err1, err2, err3, err4) \
  { const char* errs[] = {err1, err2, err3, err4, NULL}; \
    DO(test_expected_errors(src, "refer", errs)); }


class ReferTest : public PassTest
{};


TEST_F(ReferTest, ConsumeThis)
{
  const char* src =
    "class iso C\n"
      "fun iso get(): C iso^ =>\n"
        "consume this\n"

    "primitive P\n"
      "fun apply(c: C) => None\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let c = C\n"
        "(consume c).get()";

  TEST_COMPILE(src);
}

TEST_F(ReferTest, ConsumeThisReuse)
{
  const char* src =
    "class iso C\n"
      "fun iso get(): C iso^ =>\n"
        "consume this\n"
        "consume this\n"

    "primitive P\n"
      "fun apply(c: C) => None\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let c = C\n"
        "(consume c).get()";

  TEST_ERRORS_1(src,
    "can't use a consumed 'this' in an expression");
}

TEST_F(ReferTest, ConsumeLet)
{
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply(c: C) => None\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let c = C\n"
        "P(consume c)";

  TEST_COMPILE(src);
}

TEST_F(ReferTest, ConsumeLetReassign)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let c = C\n"
        "c = consume c";

  TEST_ERRORS_2(src,
    "can't reassign to a let local",
    "left side must be something that can be assigned to");
}

TEST_F(ReferTest, ConsumeLetField)
{
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply(c: C) => None\n"

    "actor Main\n"
      "let c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "P(consume c)";

  TEST_ERRORS_1(src,
    "can't consume a let or embed field");
}

TEST_F(ReferTest, ConsumeVar)
{
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply(c: C) => None\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "var c = C\n"
        "P(consume c)";

  TEST_COMPILE(src);
}

TEST_F(ReferTest, ConsumeVarReuse)
{
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply(c: C) => None\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "var c = C\n"
        "consume c\n"
        "P(consume c)";

  TEST_ERRORS_2(src,
    "can't use a consumed local or field in an expression",
    "consume must take 'this', a local, or a parameter");
}

TEST_F(ReferTest, ConsumeVarReassign)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "var c = C\n"
        "c = consume c";

  TEST_COMPILE(src);
}

TEST_F(ReferTest, ConsumeVarField)
{
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply(c: C) => None\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "P(consume c)";

  TEST_ERRORS_1(src,
    "consuming a field is only allowed if it is reassigned in the same"
    " expression");
}

TEST_F(ReferTest, ConsumeVarFieldReassign)
{
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply(c: C): C iso^ => consume c\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "c = P(consume c)";

  TEST_COMPILE(src);
}

TEST_F(ReferTest, ConsumeEmbedField)
{
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply(c: C) => None\n"

    "actor Main\n"
      "embed c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "P(consume c)";

  TEST_ERRORS_1(src,
    "can't consume a let or embed field");
}

TEST_F(ReferTest, ConsumeVarTry)
{
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply(c: C) ? => error\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "var c = C\n"
        "try\n"
        "  P(consume c) ?"
        "end";

  TEST_COMPILE(src);
}

TEST_F(ReferTest, ConsumeVarReassignTry)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "var c = C\n"
        "try\n"
        "  let d = consume c\n"
        "  c = consume d\n"
        "  error\n"
        "end\n";

  TEST_ERRORS_2(src,
    "can't reassign to a consumed identifier in a try expression unless it is"
    " reassigned in the same expression",
    "left side must be something that can be assigned to");
}

TEST_F(ReferTest, ConsumeVarFieldTry)
{
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply(c: C) ? => error\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  P(consume c) ?"
        "end";

  TEST_ERRORS_1(src,
    "consuming a field is only allowed if it is reassigned in the same"
    " expression");
}

TEST_F(ReferTest, WhileLoopConsumeLet)
{
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply(c: C) => None\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let c = C\n"
        "while true do\n"
          "P(consume c)\n"
        "end";

  TEST_ERRORS_1(src,
    "identifier 'c' is consumed when the loop exits");
}

TEST_F(ReferTest, WhileLoopConsumeLetInCondition)
{
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply(c: C) => None\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let c = C\n"
        "while P(consume c); true do\n"
          "None\n"
        "end";

  TEST_ERRORS_1(src,
    "can't consume from an outer scope in a loop condition");
}

TEST_F(ReferTest, WhileLoopDeclareLetAndUseOutside)
{
  const char* src =
    "class val C\n"
      "new val create() => None\n"

    "primitive P\n"
      "fun apply(c: C) => None\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "while let c1 = C; true do\n"
          "let c2 = C\n"
        "else\n"
          "let c3 = C\n"
        "end\n"

        "P(c1)\n"
        "P(c2)\n"
        "P(c3)";

  TEST_ERRORS_3(src,
    "can't find declaration of 'c1'",
    "can't find declaration of 'c2'",
    "can't find declaration of 'c3'");
}

TEST_F(ReferTest, RepeatLoopConsumeLet)
{
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply(c: C) => None\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let c = C\n"
        "repeat\n"
          "P(consume c)\n"
        "until false end";

  TEST_ERRORS_1(src,
    "identifier 'c' is consumed when the loop exits");
}

TEST_F(ReferTest, RepeatLoopConsumeLetInCondition)
{
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply(c: C) => None\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let c = C\n"
        "repeat\n"
          "None\n"
        "until\n"
          "P(consume c); false\n"
        "end";

  TEST_ERRORS_1(src,
    "can't consume from an outer scope in a loop condition");
}

TEST_F(ReferTest, RepeatLoopDeclareLetAndUseOutside)
{
  const char* src =
    "class val C\n"
      "new val create() => None\n"

    "primitive P\n"
      "fun apply(c: C) => None\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "repeat\n"
          "let c1 = C\n"
        "until\n"
          "let c2 = C; true\n"
        "else\n"
          "let c3 = C\n"
        "end\n"

        "P(c1)\n"
        "P(c2)\n"
        "P(c3)";

  TEST_ERRORS_3(src,
    "can't find declaration of 'c1'",
    "can't find declaration of 'c2'",
    "can't find declaration of 'c3'");
}

TEST_F(ReferTest, UndefinedVarValueNeeded)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "new create(env: Env) => None\n"
      "fun apply(): C =>\n"
        "var c = C";

  TEST_ERRORS_1(src,
    "the left side is undefined but its value is used");
}

TEST_F(ReferTest, RepeatLoopDefineVarAndUseOutside)
{
  const char* src =
    "actor Main\n"
      "new create(env: Env) =>\n"
        "var n: U8\n"
        "repeat\n"
          "n = 5\n"
        "until true end\n"
        "let n' = n";

  TEST_COMPILE(src);
}

TEST_F(ReferTest, RepeatLoopDefineVarAfterBreakAndUseOutside)
{
  const char* src =
    "actor Main\n"
      "new create(env: Env) =>\n"
        "var n: U8\n"
        "repeat\n"
          "if true then break end\n"
          "n = 5\n"
        "until true end\n"
        "let n' = n";

  TEST_ERRORS_1(src,
    "can't use an undefined variable in an expression");
}

TEST_F(ReferTest, RepeatLoopDefineVarAfterNestedBreakAndUseOutside)
{
  const char* src =
    "actor Main\n"
      "new create(env: Env) =>\n"
        "var n: U8\n"
        "repeat\n"
          "while true do break end\n"
          "n = 5\n"
        "until true end\n"
        "let n' = n";

  TEST_COMPILE(src);
}

TEST_F(ReferTest, RepeatLoopDefineVarAndUseInCondition)
{
  // From issue #2784
  const char* src =
    "actor Main\n"
      "new create(env: Env) =>\n"
        "var n: U8\n"
        "repeat\n"
          "n = 5\n"
        "until (n > 2) end";

  TEST_COMPILE(src);
}

TEST_F(ReferTest, ConsumeVarFieldReassignSameExpressionConsumedReference)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c = (consume c; consume c)\n"
        "error";

  TEST_ERRORS_1(src,
    "can't use a consumed local or field in an expression");
}

TEST_F(ReferTest, ConsumeVarFieldReassignSameExpressionReuse)
{
  const char* src =
    "class iso C\n"
      "fun ref blah(d: C): C iso^ =>\n"
        "consume d\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c = c.blah(consume c)\n"
        "error";

  TEST_ERRORS_1(src,
    "can't use a consumed local or field in an expression");
}

TEST_F(ReferTest, ConsumeVarFieldReassignSameExpressionReuseSubfield)
{
  const char* src =
    "class iso C\n"
      "let s: String\n"
      "new iso create() => s = String\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref blah(d: C, s: String): C iso^ =>\n"
        "consume d\n"

      "fun ref dostuff() ? =>\n"
        "c = blah(consume c, c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "can't use a consumed local or field in an expression");
}

TEST_F(ReferTest,
  ConsumeVarFieldVarSubfieldReassignSameExpressionConsumedReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = (consume c.s; consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "can't use a consumed local or field in an expression");
}

TEST_F(ReferTest, ConsumeVarFieldVarSubfieldReassignSameExpressionReuse)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref blah(d: Array[U8] iso, n: USize): Array[U8] iso^ =>\n"
        "consume d\n"

      "fun ref dostuff() ? =>\n"
        "c.s = blah(consume c.s, c.s.size())\n"
        "error";

  TEST_ERRORS_1(src,
    "can't use a consumed local or field in an expression");
}

TEST_F(ReferTest,
  ConsumeLetFieldVarSubfieldReassignSameExpressionConsumedReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "let c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = (consume c.s; consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "can't use a consumed local or field in an expression");
}

TEST_F(ReferTest, ConsumeLetFieldVarSubfieldReassignSameExpressionReuse)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "let c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref blah(d: Array[U8] iso, n: USize): Array[U8] iso^ =>\n"
        "consume d\n"

      "fun ref dostuff() ? =>\n"
        "c.s = blah(consume c.s, c.s.size())\n"
        "error";

  TEST_ERRORS_1(src,
    "can't use a consumed local or field in an expression");
}

TEST_F(ReferTest,
  ConsumeEmbedFieldVarSubfieldReassignSameExpressionConsumedReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "embed c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = (consume c.s; consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "can't use a consumed local or field in an expression");
}

TEST_F(ReferTest, ConsumeEmbedFieldVarSubfieldReassignSameExpressionReuse)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "embed c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref blah(d: Array[U8] iso, n: USize): Array[U8] iso^ =>\n"
        "consume d\n"

      "fun ref dostuff() ? =>\n"
        "c.s = blah(consume c.s, c.s.size())\n"
        "error";

  TEST_ERRORS_1(src,
    "can't use a consumed local or field in an expression");
}

TEST_F(ReferTest,
  ConsumeVarLocalVarSubfieldReassignSameExpressionConsumedReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "var c: C = C\n"
        "c.s = (consume c.s; consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "can't use a consumed local or field in an expression");
}

TEST_F(ReferTest, ConsumeVarLocalVarSubfieldReassignSameExpressionReuse)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref blah(d: Array[U8] iso, n: USize): Array[U8] iso^ =>\n"
        "consume d\n"

      "fun ref dostuff() ? =>\n"
        "var c: C = C\n"
        "c.s = blah(consume c.s, c.s.size())\n"
        "error";

  TEST_ERRORS_1(src,
    "can't use a consumed local or field in an expression");
}

TEST_F(ReferTest,
  ConsumeLetLocalVarSubfieldReassignSameExpressionConsumedReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "let c: C = C\n"
        "c.s = (consume c.s; consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "can't use a consumed local or field in an expression");
}

TEST_F(ReferTest, ConsumeLetLocalVarSubfieldReassignSameExpressionReuse)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref blah(d: Array[U8] iso, n: USize): Array[U8] iso^ =>\n"
        "consume d\n"

      "fun ref dostuff() ? =>\n"
        "let c: C = C\n"
        "c.s = blah(consume c.s, c.s.size())\n"
        "error";

  TEST_ERRORS_1(src,
    "can't use a consumed local or field in an expression");
}

TEST_F(ReferTest, ConsumeParamVarSubfieldReassignSameExpressionConsumedReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff(C)?\n"
        "end\n"

      "fun ref dostuff(c: C) ? =>\n"
        "c.s = (consume c.s; consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "can't use a consumed local or field in an expression");
}

TEST_F(ReferTest, ConsumeParamVarSubfieldReassignSameExpressionReuse)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff(C)?\n"
        "end\n"

      "fun ref blah(d: Array[U8] iso, n: USize): Array[U8] iso^ =>\n"
        "consume d\n"

      "fun ref dostuff(c: C) ? =>\n"
        "c.s = blah(consume c.s, c.s.size())\n"
        "error";

  TEST_ERRORS_1(src,
    "can't use a consumed local or field in an expression");
}

TEST_F(ReferTest, ConsumeTupleAccessorOfFunctionValResult)
{
  const char* src =
    "actor Main\n"
      "new create(env: Env) =>\n"
        "let a = \"I am a string\"\n"
        "consume a.chop(1)._1";

  TEST_ERRORS_1(src,
    "Consume expressions must specify an identifier or field");
}

TEST_F(ReferTest, DoubleConsumeWithFieldAccessor)
{
  const char* src =
    "class Problem\n"
      "let _unwrap_me: U8 = 0\n"
      "fun iso unwrap() =>\n"
        "let this_iso: Problem iso = consume this\n"
        "consume (consume this_iso)._unwrap_me";

  TEST_ERRORS_1(src,
    "Consume expressions must specify an identifier or field");
}

TEST_F(ReferTest, ConsumeAfterMemberAccessWithConsumeLhs)
{
  const char* src =
    "actor Main\n"
      "new create(env:Env val) =>\n"
        "let f : Foo iso = recover Foo end\n"
        "(consume f).b = recover Bar end\n"
        "let x:Foo tag = consume f\n"
    "class Foo\n"
      "var b: Bar iso\n"
      "new create() =>\n"
        "b = recover Bar end\n"
    "class Bar\n"
      "new create() => None\n";

    TEST_ERRORS_2(src,
      "can't use a consumed local or field in an expression",
      "consume must take 'this', a local, or a parameter");
}

TEST_F(ReferTest, ConsumeInvalidExpression)
{
  // From issue #4477
  const char *src =
    "struct FFIBytes\n"
      "var ptr: Pointer[U8 val] = Pointer[U8].create()\n"
      "var length: USize = 0\n"
      "fun iso string(): String val =>\n"
        "recover String.from_cpointer(consume FFIBytes.ptr,"
                                     "consume FFIBytes.length) end\n"
    "actor Main\n"
      "new create(env: Env) =>\n"
        "env.out.print(\"nothing to see here\")\n";

  TEST_ERRORS_4(src,
        "You can't consume an expression that isn't local. More specifically,"
        " you can only consume a local variable (a single lowercase"
        " identifier, with no dots) or a field of this (this followed by a dot"
        " and a single lowercase identifier).",
        "consuming a field is only allowed if it is reassigned in the same"
        " expression",
        "You can't consume an expression that isn't local. More specifically,"
        " you can only consume a local variable (a single lowercase"
        " identifier, with no dots) or a field of this (this followed by a dot"
        " and a single lowercase identifier).",
        "consuming a field is only allowed if it is reassigned in the same"
        " expression");
}

TEST_F(ReferTest, MemberAccessWithConsumeLhs)
{
  const char* src =
    "actor Main\n"
      "new create(env:Env val) =>\n"
        "let f : Foo iso = recover Foo end\n"
        "(consume f).b = recover Bar end\n"
    "class Foo\n"
      "var b: Bar iso\n"
        "new create() =>\n"
          "b = recover Bar end\n"
    "class Bar\n"
      "new create() => None\n";

  TEST_COMPILE(src);
}

TEST_F(ReferTest, TraitDefaultBodyWithAbstractTraitParam)
{
  // From issue #5833
  const char* src =
    "trait Concrete\n"
      "fun hello(env: Env) =>\n"
        "env.out.print(\"hello\")\n"

    "trait Abstract1\n"
      "fun hello(env: Env)\n"

    "actor Main is (Abstract1 & Concrete)\n"
      "new create(env: Env) =>\n"
        "hello(env)";

  TEST_COMPILE(src);
}


TEST_F(ReferTest, ConsumeAfterErrorInTryElse)
{
  // Issue #862: consume after the last error point in a try body should
  // not prevent using the variable in the else clause.
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply() ? => error\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let c = C\n"
        "try\n"
        "  P() ?\n"
        "  let d = consume c\n"
        "else\n"
        "  let d = consume c\n"
        "end";

  TEST_COMPILE(src);
}


TEST_F(ReferTest, ConsumeBeforeErrorInTryElse)
{
  // Consuming a variable before the error point means it IS consumed
  // when entering the else clause.
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply(c: C) ? => error\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let c = C\n"
        "try\n"
        "  P(consume c) ?\n"
        "else\n"
        "  let d = consume c\n"
        "end";

  TEST_ERRORS_2(src,
    "can't use a consumed local or field in an expression",
    "consume must take 'this', a local, or a parameter");
}


TEST_F(ReferTest, ConsumeAfterMultipleErrorsInTryElse)
{
  // Variable consumed after all error points is available in else.
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply() ? => error\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let c = C\n"
        "try\n"
        "  P() ?\n"
        "  P() ?\n"
        "  let d = consume c\n"
        "else\n"
        "  let d = consume c\n"
        "end";

  TEST_COMPILE(src);
}


TEST_F(ReferTest, ConsumeAndReassignBetweenErrorsInTryElse)
{
  // Variable consumed and reassigned (same expression) between two error
  // points is still defined at both — else clause can use it.
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply() ? => error\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "var c = C\n"
        "try\n"
        "  P() ?\n"
        "  c = consume c\n"
        "  P() ?\n"
        "else\n"
        "  let d = consume c\n"
        "end";

  TEST_COMPILE(src);
}


TEST_F(ReferTest, ConsumeAfterErrorInNestedIfTryElse)
{
  // Consume inside a nested if after the last error point is still
  // unreachable on the error path.
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply() ? => error\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "var c = C\n"
        "try\n"
        "  P() ?\n"
        "  if true then\n"
        "    c = consume c\n"
        "  end\n"
        "else\n"
        "  let d = consume c\n"
        "end";

  TEST_COMPILE(src);
}


TEST_F(ReferTest, ConsumeAfterErrorKeywordInTryElse)
{
  // An `error` keyword inside a conditional is an error point; consumes
  // after it are unreachable on the error path.
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "foo(true)\n"

      "fun foo(cond: Bool) =>\n"
        "let c = C\n"
        "try\n"
        "  if cond then error end\n"
        "  let d = consume c\n"
        "else\n"
        "  let d = consume c\n"
        "end";

  TEST_COMPILE(src);
}


TEST_F(ReferTest, ConsumeAfterAsCastInTryElse)
{
  // An `as` cast is a syntactic error point; consumes after it are
  // unreachable on the error path.
  const char* src =
    "trait T\n"
    "class iso C is T\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "var c: T = C\n"
        "var d = C\n"
        "try\n"
        "  c = c as C\n"
        "  let e = consume d\n"
        "else\n"
        "  let e = consume d\n"
        "end";

  TEST_COMPILE(src);
}


TEST_F(ReferTest, TwoVarsConsumedAtDifferentPointsInTryElse)
{
  // x consumed before the error point is rejected in else; y consumed
  // after the error point is available.
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply(c: C) ? => error\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let x = C\n"
        "let y = C\n"
        "try\n"
        "  P(consume x) ?\n"
        "  let z = consume y\n"
        "else\n"
        "  let z = consume x\n"
        "end";

  TEST_ERRORS_2(src,
    "can't use a consumed local or field in an expression",
    "consume must take 'this', a local, or a parameter");
}


TEST_F(ReferTest, ConsumeAfterNestedTryInTryElse)
{
  // An inner try swallows its own errors — it is not an error point
  // for the outer try.
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply() ? => error\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let c = C\n"
        "try\n"
        "  P() ?\n"
        "  try P() ? end\n"
        "  let d = consume c\n"
        "else\n"
        "  let d = consume c\n"
        "end";

  TEST_COMPILE(src);
}


TEST_F(ReferTest, ConsumeParamAfterErrorInTryElse)
{
  // Parameter consumed after the last error point is available in else.
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply() ? => error\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "foo(C)\n"

      "fun foo(c: C iso) =>\n"
        "try\n"
        "  P() ?\n"
        "  let d = consume c\n"
        "else\n"
        "  let d = consume c\n"
        "end";

  TEST_COMPILE(src);
}


TEST_F(ReferTest, ConsumeOnMutuallyExclusiveBranchFromErrorInTryElse)
{
  // Error and consume are in different branches of an if inside try.
  // The consume cannot have executed when the error fires, so the
  // variable is available in else.
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply() ? => error\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "foo(true)\n"

      "fun foo(cond: Bool) =>\n"
        "let c = C\n"
        "try\n"
        "  if cond then P() ? else let d = consume c end\n"
        "else\n"
        "  let d = consume c\n"
        "end";

  TEST_COMPILE(src);
}


TEST_F(ReferTest, ConsumeOnSameBranchAsErrorInTryElse)
{
  // Consume and error on the same branch — the consume happens before
  // the error, so the variable is consumed at the error point and
  // must be rejected in else.
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply(c: C) ? => error\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "foo(true)\n"

      "fun foo(cond: Bool) =>\n"
        "let c = C\n"
        "try\n"
        "  if cond then P(consume c) ? end\n"
        "else\n"
        "  let d = consume c\n"
        "end";

  TEST_ERRORS_2(src,
    "can't use a consumed local or field in an expression",
    "consume must take 'this', a local, or a parameter");
}


TEST_F(ReferTest, ConsumeInConditionWithErrorInBranchTryElse)
{
  // Consume in the condition of an if; error in a branch.
  // The condition executes before either branch, so the variable
  // is consumed at the error point.
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply() ? => error\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "foo(C)\n"

      "fun foo(c: C iso) =>\n"
        "try\n"
        "  let cond = (consume c; true)\n"
        "  if cond then P() ? end\n"
        "else\n"
        "  let d = consume c\n"
        "end";

  TEST_ERRORS_2(src,
    "can't use a consumed local or field in an expression",
    "consume must take 'this', a local, or a parameter");
}


TEST_F(ReferTest, ConsumeInAsCastAtErrorPointTryElse)
{
  // A variable consumed inside an `as` expression is consumed at the
  // error point (as can error), so the else clause cannot use it.
  const char* src =
    "trait T\n"
    "class iso C is T\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let c: (T | C) = C\n"
        "let d = C\n"
        "try\n"
        "  let e = (consume d) as C\n"
        "else\n"
        "  let f = consume d\n"
        "end";

  TEST_ERRORS_2(src,
    "can't use a consumed local or field in an expression",
    "consume must take 'this', a local, or a parameter");
}


TEST_F(ReferTest, ConsumeThisAfterErrorInTryElse)
{
  // `consume this` after the last error point is unreachable on the
  // error path, so `this` is available in the else clause.
  const char* src =
    "class iso C\n"
      "var x: U32 = 0\n"
      "fun iso foo(): (C iso^ | None) =>\n"
        "try\n"
        "  P() ?\n"
        "  consume this\n"
        "else\n"
        "  x = 42\n"
        "  None\n"
        "end\n"

    "primitive P\n"
      "fun apply() ? => error\n"

    "actor Main\n"
      "new create(env: Env) => None";

  TEST_COMPILE(src);
}


TEST_F(ReferTest, ConsumeInMatchCaseWithErrorInOtherCaseTryElse)
{
  // Error and consume are in different match cases inside try.
  // Cases are mutually exclusive, so the variable is available in else.
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply() ? => error\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "foo(U32(1))\n"

      "fun foo(x: (U32 | String val)) =>\n"
        "let c = C\n"
        "try\n"
        "  match x\n"
        "  | let n: U32 => P() ?\n"
        "  | let s: String val => let d = consume c\n"
        "  end\n"
        "else\n"
        "  let d = consume c\n"
        "end";

  TEST_COMPILE(src);
}


TEST_F(ReferTest, ConsumeAfterWhileWithErrorInTryElse)
{
  // Error is inside a while body, consume is after the while.
  // The while can error, so `c` isn't consumed on the error path.
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply() ? => error\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let c = C\n"
        "try\n"
        "  while true do\n"
        "    P() ?\n"
        "    break\n"
        "  end\n"
        "  let d = consume c\n"
        "else\n"
        "  let d = consume c\n"
        "end";

  TEST_COMPILE(src);
}


TEST_F(ReferTest, ConsumeAfterRepeatWithErrorInTryElse)
{
  // Error is inside a repeat body, consume is after the repeat.
  // The repeat can error, so `c` isn't consumed on the error path.
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply() ? => error\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let c = C\n"
        "try\n"
        "  repeat\n"
        "    P() ?\n"
        "    break\n"
        "  until true end\n"
        "  let d = consume c\n"
        "else\n"
        "  let d = consume c\n"
        "end";

  TEST_COMPILE(src);
}
