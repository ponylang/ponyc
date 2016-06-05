#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"

/** Tests for logic in the reference resolution pass (PASS_REFERENCE)
 */


#define TEST_COMPILE(src) DO(test_compile(src, "all"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "reference", errs)); }

#define TEST_ERRORS_2(src, err1, err2) \
  { const char* errs[] = {err1, err2, NULL}; \
    DO(test_expected_errors(src, "reference", errs)); }

#define TEST_ERRORS_3(src, err1, err2, err3) \
  { const char* errs[] = {err1, err2, err3, NULL}; \
    DO(test_expected_errors(src, "reference", errs)); }


class ReferenceTest : public PassTest
{};


TEST_F(ReferenceTest, CantResolveReference)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    bogus";

  TEST_ERRORS_1(src, "can't find declaration of 'bogus'");
}


TEST_F(ReferenceTest, CantResolveReferenceSuggestLeadingUnderscore)
{
  const char* src =
    "actor Main\n"
    "  let _name: I32 = 0\n"
    "  new create(env: Env) =>\n"
    "    name";

  TEST_ERRORS_1(src, "can't find declaration of 'name', did you mean '_name'?");
}


TEST_F(ReferenceTest, CantResolveReferenceSuggestWithoutLeadingUnderscore)
{
  const char* src =
    "actor Main\n"
    "  let name: I32 = 0\n"
    "  new create(env: Env) =>\n"
    "    _name";

  TEST_ERRORS_1(src, "can't find declaration of '_name', did you mean 'name'?");
}


TEST_F(ReferenceTest, CantResolveReferenceSuggestDifferentCase)
{
  const char* src =
    "actor Main\n"
    "  let name: I32 = 0\n"
    "  new create(env: Env) =>\n"
    "    nAmE";

  TEST_ERRORS_1(src, "can't find declaration of 'nAmE', did you mean 'name'?");
}


TEST_F(ReferenceTest, CantResolveReferenceDontSuggestTypeCase)
{
  const char* src =
    "primitive Name\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    name";

  TEST_ERRORS_1(src, "can't find declaration of 'name'");
}


TEST_F(ReferenceTest, CantResolveType)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bogus";

  TEST_ERRORS_1(src, "can't find declaration of 'Bogus'");
}


TEST_F(ReferenceTest, CantResolveTypeSuggestLeadingUnderscore)
{
  const char* src =
    "primitive _Name\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Name";

  TEST_ERRORS_1(src, "can't find declaration of 'Name', did you mean '_Name'?");
}


TEST_F(ReferenceTest, CantResolveTypeSuggestWithoutLeadingUnderscore)
{
  const char* src =
    "primitive Name\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    _Name";

  TEST_ERRORS_1(src, "can't find declaration of '_Name', did you mean 'Name'?");
}


TEST_F(ReferenceTest, CantResolveTypeSuggestDifferentCase)
{
  const char* src =
    "primitive Name\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    NaMe";

  TEST_ERRORS_1(src, "can't find declaration of 'NaMe', did you mean 'Name'?");
}


TEST_F(ReferenceTest, CantResolveTypeDontSuggestReferenceCase)
{
  const char* src =
    "actor Main\n"
    "  let name: I32 = 0\n"
    "  new create(env: Env) =>\n"
    "    Name";

  TEST_ERRORS_1(src, "can't find declaration of 'Name'");
}
