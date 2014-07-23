extern "C" {
#include "../../src/libponyc/ast/ast.h"
#include "../../src/libponyc/pass/names.h"
}
#include "util.h"
#include <gtest/gtest.h>


class FlattenTest: public testing::Test
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
