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


class BadPonyTest : public PassTest
{
protected:
  void test(const char* src)
  {
    DO(test_error(src, "ir"));
    ASSERT_NE(0, get_error_count());
  }
};


// Cases from reported issues

TEST_F(BadPonyTest, DontCareInFieldType)
{
  // From issue #207
  const char* src =
    "class Abc\n"
    "  var _test1: (_)";

  DO(test(src));
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

  DO(test(src));
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

  DO(test(src));
}


TEST_F(BadPonyTest, DontCareInIntLiteralType)
{
  // From issue #222
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let crashme: (_, I32) = (4, 4)";

  DO(test(src));
}
