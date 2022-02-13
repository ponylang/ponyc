#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>
#include <ast/error.h>
#include <pass/names.h>

#include "util.h"

#define TEST_ERROR(src) DO(test_error(src, "flatten"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "expr", errs)); }


class FlattenTest: public PassTest
{};

/*
static void test(const char* before, const char* after, token_id start_id)
{
  test_good_pass(before, after, start_id, pass_flatten);
}


TEST(FlattenTest, Union)
{
  const char* before =
    "(uniontype"
    "  (nominal x (id A) x ref x)"
    "  (uniontype"
    "    (nominal x (id B) x iso x)"
    "    (nominal x (id C) x tag x))"
    "  (uniontype"
    "    (nominal x (id D) x trn x)"
    "    (nominal x (id E) x box x)))";

  const char* after =
    "(uniontype"
    "  (nominal x (id A) x ref x)"
    "  (nominal x (id B) x iso x)"
    "  (nominal x (id C) x tag x)"
    "  (nominal x (id D) x trn x)"
    "  (nominal x (id E) x box x))";

  ASSERT_NO_FATAL_FAILURE(test(before, after, TK_UNIONTYPE));
}
*/


TEST_F(FlattenTest, TypeparamCap)
{
  const char* src =
    "class C\n"
    "  fun foo[A]() =>\n"
    "    let a: A ref";

  TEST_ERROR(src);
}
TEST_F(FlattenTest, SendableParamConstructor)
{
  const char* src =
    "class X\n"
    "  fun gimme(iso_lambda: {(None): None} iso) =>\n"
    "    None\n"
    "\n"
    "class Y\n"
    "class UseX\n"
    "  fun doit() =>\n"
    "    let y: Y ref = Y\n"
    "    X.gimme({(x: None) => y.size() })";
  TEST_ERRORS_1(src, "this parameter must be sendable (iso, val or tag)");
  ASSERT_EQ(1, errors_get_count(opt.check.errors));
  errormsg_t* errormsg = errors_get_first(opt.check.errors);

  ASSERT_EQ(9, errormsg->line);
  ASSERT_EQ(27, errormsg->pos);
}
