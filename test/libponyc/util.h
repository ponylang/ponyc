#ifndef UNIT_UTIL_H
#define UNIT_UTIL_H

#include <platform.h>

#include <ast/ast.h>
#include <ast/builder.h>
#include <pass/pass.h>

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

void load_test_program(const char* pass, const char* name, ast_t** out_prog);

/** Check that the given AST matches the given description. Siblings of the AST
 * are ignored.
 * Errors are checked with ASSERTs, call in ASSERT_NO_FATAL_FAILURE.
 */
void check_tree(const char* expected, ast_t* actual);


class PassTest : public testing::Test
{
protected:
  ast_t* program; // AST produced from given source
  ast_t* package; // AST of first package, cache of ast_child(program)

  virtual void SetUp();
  virtual void TearDown();

  // Override the default builtin source
  void set_builtin(const char* src);

  // Add an additional package source
  void add_package(const char* path, const char* src);

  // Override the default path of the main package (useful for package loops)
  void default_package_name(const char* path);


  // Test methods.
  // These all check error with ASSERTs, call in ASSERT_NO_FATAL_FAILURE.

  // Check that the given source compiles to the specified pass without error
  void test_compile(const char* src, const char* pass);

  // Check that the given source fails when compiled to the specified pass
  void test_error(const char* src, const char* pass);

  // Check that the 2 given sources compile to give the same AST for the first
  // package
  void test_equiv(const char* actual_src, const char* actual_pass,
    const char* expect_src, const char* expect_pass);

  // Check that the given source compiles to give the described AST for the
  // whole program
  void test_program_ast(const char* src, const char* pass, const char* desc);

  // Check that the given source compiles to give the described AST for the
  // first package
  void test_package_ast(const char* src, const char* pass, const char* desc);

private:
  const char* _builtin_src;
  const char* _first_pkg_path;
};


#endif
