extern "C" {
#include "../../src/libponyc/ast/ast.h"
}


ast_t* find_start(ast_t* tree, token_id start_id);

void test_good_pass(const char* before, const char* after, token_id start_id,
  ast_result_t (*pass_fn)(ast_t**));

void test_bad_pass(const char* desc, token_id start_id,
  ast_result_t expect_result, ast_result_t (*pass_fn)(ast_t**));
