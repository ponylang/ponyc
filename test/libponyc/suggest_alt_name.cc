#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"

#define TEST_ERRORS(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "ir", errs)); }

class SuggestAltNameTest : public PassTest
{};

TEST_F(SuggestAltNameTest, SuggestAltName_Private)
{
  const char* src =
    "actor Main\n"
    "  var _aStr: String = \"help\"\n"

    "  new create(env: Env) =>\n"
    "    _aStr = xx()\n"

    "  fun xx(): String =>\n"
    "    aStr // Undefined suggest _aStr\n";

  TEST_ERRORS(src,
    "can't find declaration of 'aStr', did you mean '_aStr'?");
}

TEST_F(SuggestAltNameTest, SuggestAltName_DifferentCaseVariable)
{
  const char* src =
    "actor Main\n"
    "  var aStr: String = \"help\"\n"

    "  new create(env: Env) =>\n"
    "    aStr = xx()\n"

    "  fun xx(): String =>\n"
    "    astr // Undefined, suggest aStr\n";

  TEST_ERRORS(src,
    "can't find declaration of 'astr', did you mean 'aStr'?");
}

TEST_F(SuggestAltNameTest, SuggestAltName_DifferentCaseMethod)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    doit() // Undefined, suggest doIt\n"

    "  fun doIt() =>\n"
    "    None\n";

  TEST_ERRORS(src,
    "can't find declaration of 'doit', did you mean 'doIt'?");
}

TEST_F(SuggestAltNameTest, SuggestAltName_NothingToSuggest)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    doIt() // Undefined, nothing to suggest\n";

  TEST_ERRORS(src,
    "can't find declaration of 'doIt'");
}
