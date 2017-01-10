#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"


/** Pony code that parses, but is erroneous. Typically type check errors and
 * things used in invalid contexts.
 *
 * We build all the way up to and including code gen and check that we do not
 * assert, segfault, etc but that the build fails and at least one error is
 * reported.
 *
 * There is definite potential for overlap with other tests but this is also a
 * suitable location for tests which don't obviously belong anywhere else.
 */

#define TEST_COMPILE(src) DO(test_compile(src, "all"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "ir", errs)); }

#define TEST_ERRORS_2(src, err1, err2) \
  { const char* errs[] = {err1, err2, NULL}; \
    DO(test_expected_errors(src, "ir", errs)); }

#define TEST_ERRORS_3(src, err1, err2, err3) \
  { const char* errs[] = {err1, err2, err3, NULL}; \
    DO(test_expected_errors(src, "ir", errs)); }


class BadPonyTest : public PassTest
{};


// Cases from reported issues

TEST_F(BadPonyTest, ClassInOtherClassProvidesList)
{
  // From issue #218
  const char* src =
    "class Named\n"
    "class Dog is Named\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src, "can only provide traits and interfaces");
}


TEST_F(BadPonyTest, TypeParamMissingForTypeInProvidesList)
{
  // From issue #219
  const char* src =
    "trait Bar[A]\n"
    "  fun bar(a: A) =>\n"
    "    None\n"

    "trait Foo is Bar // here also should be a type argument, like Bar[U8]\n"
    "  fun foo() =>\n"
    "    None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src, "not enough type arguments");
}

TEST_F(BadPonyTest, TupleIndexIsZero)
{
  // From issue #397
  const char* src =
    "primitive Foo\n"
    "  fun bar(): None =>\n"
    "    (None, None)._0";

  TEST_ERRORS_1(src, "Did you mean _1?");
}

TEST_F(BadPonyTest, TupleIndexIsOutOfRange)
{
  // From issue #397
  const char* src =
    "primitive Foo\n"
    "  fun bar(): None =>\n"
    "    (None, None)._3";

  TEST_ERRORS_1(src, "Valid range is [1, 2]");
}

TEST_F(BadPonyTest, InvalidLambdaReturnType)
{
  // From issue #828
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    {(): tag => this }\n";

  TEST_ERRORS_1(src, "lambda return type: tag");
}

TEST_F(BadPonyTest, InvalidMethodReturnType)
{
  // From issue #828
  const char* src =
    "primitive Foo\n"
    "  fun bar(): iso =>\n"
    "    U32(1)\n";

  TEST_ERRORS_1(src, "function return type: iso");
}

TEST_F(BadPonyTest, ObjectLiteralUninitializedField)
{
  // From issue #879
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    object\n"
    "      let x: I32\n"
    "    end";

  TEST_ERRORS_1(src, "object literal fields must be initialized");
}

TEST_F(BadPonyTest, LambdaCaptureVariableBeforeDeclarationWithTypeInferenceExpressionFail)
{
  // From issue #1018
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    {()(x) => None }\n"
    "    let x = 0";

   TEST_ERRORS_1(src, "declaration of 'x' appears after use");
}

// TODO: This test is not correct because it does not fail without the fix.
// I do not know how to generate a test that calls genheader().
// Comments are welcomed.
TEST_F(BadPonyTest, ExportedActorWithVariadicReturnTypeContainingNone)
{
  // From issue #891
  const char* src =
    "primitive T\n"
    "\n"
    "actor @A\n"
    "  fun f(a: T): (T | None) =>\n"
    "    a\n";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, TypeAliasRecursionThroughTypeParameterInTuple)
{
  // From issue #901
  const char* src =
    "type Foo is (Map[Foo, Foo], None)\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src, "type aliases can't be recursive");
}

TEST_F(BadPonyTest, ParenthesisedReturn)
{
  // From issue #1050
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    (return)";

  TEST_ERRORS_1(src, "use return only to exit early from a method");
}

TEST_F(BadPonyTest, ParenthesisedReturn2)
{
  // From issue #1050
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo()\n"

    "  fun foo(): U64 =>\n"
    "    (return 0)\n"
    "    2";

  TEST_ERRORS_1(src, "Unreachable code");
}

TEST_F(BadPonyTest, MatchUncalledMethod)
{
  // From issue #903
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    match foo\n"
    "    | None => None\n"
    "    end\n"

    "  fun foo() =>\n"
    "    None";

  TEST_ERRORS_2(src, "can't reference a method without calling it",
                     "this pattern can never match");
}

TEST_F(BadPonyTest, TupleFieldReassign)
{
  // From issue #1101
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    var foo: (U64, String) = (42, \"foo\")\n"
    "    foo._2 = \"bar\"";

  TEST_ERRORS_2(src, "can't assign to an element of a tuple",
                     "left side must be something that can be assigned to");
}

TEST_F(BadPonyTest, WithBlockTypeInference)
{
  // From issue #1135
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    with x = 1 do None end";

  TEST_ERRORS_3(src, "could not infer literal type, no valid types found",
                     "cannot infer type of $1$0",
                     "cannot infer type of x");
}

TEST_F(BadPonyTest, EmbedNestedTuple)
{
  // From issue #1136
  const char* src =
    "class Foo\n"
    "  fun get_foo(): Foo => Foo\n"

    "actor Main\n"
    "  embed foo: Foo\n"
    "  let x: U64\n"

    "  new create(env: Env) =>\n"
    "    (foo, x) = (Foo.get_foo(), 42)";

  TEST_ERRORS_1(src, "an embedded field must be assigned using a constructor");
}

TEST_F(BadPonyTest, CircularTypeInfer)
{
  // From issue #1334
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = x.create()\n"
    "    let y = y.create()";

  TEST_ERRORS_2(src, "cannot infer type of x",
                     "cannot infer type of y");
}

TEST_F(BadPonyTest, CallConstructorOnTypeIntersection)
{
  // From issue #1398
  const char* src =
    "interface Foo\n"

    "type Isect is (None & Foo)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Isect.create()";

  TEST_ERRORS_1(src, "can't call a constructor on a type intersection");
}

TEST_F(BadPonyTest, AssignToFieldOfIso)
{
  // From issue #1469
  const char* src =
    "class Foo\n"
    "  var x: String ref = String\n"
    "  fun iso bar(): String iso^ =>\n"
    "    let s = recover String end\n"
    "    x = s\n"
    "   consume s\n"

    "  fun ref foo(): String iso^ =>\n"
    "    let s = recover String end\n"
    "    let y: Foo iso = Foo\n"
    "    y.x = s\n"
    "    consume s";

  TEST_ERRORS_2(src,
    "right side must be a subtype of left side",
    "right side must be a subtype of left side"
    );
}

TEST_F(BadPonyTest, IndexArrayWithBrackets)
{
  // From issue #1493
  const char* src =
    "actor Main\n"
      "new create(env: Env) =>\n"
        "let xs = [as I64: 1, 2, 3]\n"
        "xs[1]";

  TEST_ERRORS_1(src, "Value formal parameters not yet supported");
}
