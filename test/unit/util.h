#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/ast.h"
PONY_EXTERN_C_END


ast_t* find_start(ast_t* tree, token_id start_id);

ast_t* find_sub_tree(ast_t* tree, token_id sub_id);

ast_t* parse_test_module(const char* desc);

void symtab_entry(ast_t* tree, const char* name, ast_t* expected);

void test_good_pass(const char* before, const char* after, token_id start_id,
  ast_result_t (*pass_fn)(ast_t**));

void test_bad_pass(const char* desc, token_id start_id,
  ast_result_t expect_result, ast_result_t (*pass_fn)(ast_t**));
