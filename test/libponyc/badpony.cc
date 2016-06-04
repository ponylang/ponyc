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

TEST_F(BadPonyTest, DontCareInFieldType)
{
  // From issue #207
  const char* src =
    "class Abc\n"
    "  var _test1: (_)";

  TEST_ERRORS_3(src,
    "the don't care token can only appear in",
    "field left undefined in constructor",
    "constructor with undefined fields");
}


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


TEST_F(BadPonyTest, DontCareInIntLiteralType)
{
  // From issue #222
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let crashme: (_, I32) = (4, 4)";

  TEST_ERRORS_2(src,
    "the don't care token can only appear in",
    "could not infer literal type, no valid types found");
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
    "    lambda (): tag => this end\n";

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
