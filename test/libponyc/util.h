#ifndef UNIT_UTIL_H
#define UNIT_UTIL_H

#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>
#include <ast/builder.h>
#include <pass/pass.h>

// Provide a short alias for ASSERT_NO_FATAL_FAILURE
#define DO(...) ASSERT_NO_FATAL_FAILURE(__VA_ARGS__)

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
  ast_t* module;  // AST of first module in first package
  ast_t* walk_ast; // AST walked to

  virtual void SetUp();
  virtual void TearDown();

  // Override the default builtin source
  void set_builtin(const char* src);

  // Add an additional package source
  void add_package(const char* path, const char* src);

  // Override the default path of the main package (useful for package loops)
  void default_package_name(const char* path);

  // Count the number of symbol table references to the specified name in all
  // symbol tables in the given tree
  size_t ref_count(ast_t* ast, const char* name);


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

  // Check that the given node is the specified ID and has the specified ID
  //void check_node(token_id id, const char* name, ast_t* ast);

  // Check that looking up the given name on the specified AST gives a node of
  // the specified type with the given name.
  // Walk to the newly looked up node.
  void lookup(ast_t* ast, const char* name, token_id id);

  // Check that looking up the given name on the specified AST gives NULL
  void lookup_null(ast_t* ast, const char* name);

  // Walk to the specified child of the current node
  void child(size_t index);


private:
  const char* _builtin_src;
  const char* _first_pkg_path;
};


// Macros to walk an AST and check the resulting node

// Start a tree walk
#define WALK_TREE(start) walk_ast = (start)

// Walk to the specified symbol table entry at the current node. The specified
// target must exist and must be of the specified token id.
#define LOOKUP(name, id) DO(lookup(walk_ast, name, id))

// Check that there is no symbol table entry for the specified name at the
// current node
#define LOOKUP_NULL(name) DO(lookup_null(walk_ast, name))

// Walk to the specified child index of the current node
#define CHILD(n) DO(child(n))

// Walk to the body of the current node, which must be a method
#define METHOD_BODY CHILD(6)


#endif
