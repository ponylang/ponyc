#ifndef UNIT_UTIL_H
#define UNIT_UTIL_H

extern "C" {
#include "../../src/libponyc/ast/ast.h"
#include "../../src/libponyc/ast/builder.h"
}

// Provide a short alias for ASSERT_NO_FATAL_FAILURE
#define DO(...) ASSERT_NO_FATAL_FAILURE(__VA_ARGS__)

/// Find the first occurance of the specified node ID in the given tree.
/// @return The found sub tree or NULL if not found
ast_t* find_sub_tree(ast_t* tree, token_id sub_id);


/** Build an AST based on the given description, checking for errors.
 * Errors are checked with ASSERTs, call in ASSERT_NO_FATAL_FAILURE.
 *
 * @param out_ast The generated AST.
 *
 * @param out_builder The builder used to create the returned AST. Must be
 *   freed later with builder_free().
 */
void build_ast_from_string(const char* desc, ast_t** out_ast,
  builder_t** out_builder);


/** Check that the symbol table of the given AST has an entry for the specified
 * name matching the given AST.
 * Errors are checked with ASSERTs, call in ASSERT_NO_FATAL_FAILURE.
 */
void check_symtab_entry(ast_t* scope, const char* name, const char* expected);

void check_symtab_entry(builder_t* builder, const char* scope,
  const char* name, const char* expected);


/** Check that the symbol table of the given AST has an entry for the specified
* name matching the given AST description.
* Errors are checked with ASSERTs, call in ASSERT_NO_FATAL_FAILURE.
*/
//void check_symtab_entry_desc(ast_t* tree, const char* name,
//  const char* expected);

/** Check that the given AST matches the given description. Siblings of the AST
 * are ignored.
 * Errors are checked with ASSERTs, call in ASSERT_NO_FATAL_FAILURE.
 */
//void check_tree(const char* expected, ast_t* actual);

/** Check that the given source code compiles, generating the specified result.
 * Errors are checked with ASSERTs, call in ASSERT_NO_FATAL_FAILURE.
 *
 * @param source Code to compile.
 * @param builtin Code to use for builtin package. NULL indicates use real
 * builtin.
 * @param pass Pass to limit compilation to, as defined in pass.h. NULL
 * indicates all passes.
 * @param expect Result we expect to see, checked with ASSERT.
 */
//void test_compile(const char* source, const char* builtin, const char* pass,
//  ast_result_t expect);

/** Check that the given source code compiles without error and that the
 * resulting AST matches the given description.
 * Errors are checked with ASSERTs, call in ASSERT_NO_FATAL_FAILURE.
 *
 * @param source Code to compile.
 * @param builtin Code to use for builtin package. NULL indicates use real
 * builtin.
 * @param pass Pass to limit compilation to, as defined in pass.h. NULL
 * indicates all passes.
 * @param sub_tree_id ID of the resulting subtree to check.
 * @param expect Description of the AST to expect for the specified subtree.
 */
//void test_compile_and_check_ast(const char* source, const char* builtin,
//  const char* pass, token_id sub_tree_id, const char* expect);

/** Check that the given source code compiles without error and that the
* type of the resulting AST matches the given description.
* Errors are checked with ASSERTs, call in ASSERT_NO_FATAL_FAILURE.
*
* @param source Code to compile.
* @param builtin Code to use for builtin package. NULL indicates use real
* builtin.
* @param pass Pass to limit compilation to, as defined in pass.h. NULL
* indicates all passes.
* @param sub_tree_id ID of the resulting subtree to check the type of.
* @param expect Description of the AST to expect for the specified subtree
* type.
*/
//void test_compile_and_check_type(const char* source, const char* builtin,
//  const char* pass, token_id sub_tree_id, const char* expect);

#endif


#if 0

ast_replace:
build ast fragment from desc
compare asts

colour:
build module ast from desc (handling src deletion)
  could use source


lexer tests:
do tidy up in tear down function
have next_token test function, etc

parserapi test:
similar to lexer tests
compare existing ast to desc (handling src deletion)

scope:
compile until scope1 (check good)
run pass scope1
find sub tree(id)
find and check symbtab entry, present + not

sugar:
build ast fragment from desc
perform full pass
compare asts

type check:
build module ast from desc with symbtab entries (handling src deletion)
build ast fragment
find test node
add fragment to module
compare node type to desc (handling src deletion)

#endif