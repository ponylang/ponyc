#ifndef UNIT_UTIL_H
#define UNIT_UTIL_H

#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>

// Provide a short alias for ASSERT_NO_FATAL_FAILURE
#define DO(...) ASSERT_NO_FATAL_FAILURE(__VA_ARGS__)

// Assert that the given node exists and is the specified id
#define ASSERT_ID(expected, ast) \
  { \
    ast_t* t = (ast); \
    ASSERT_NE((void*)NULL, t); \
    ASSERT_EQ(expected, ast_id(t)); \
  }


class PassTest : public testing::Test
{
protected:
  ast_t* program; // AST produced from given source
  ast_t* package; // AST of first package, cache of ast_child(program)
  ast_t* module;  // AST of first module in first package

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

  // Check that the 2 given chunks of AST are the same. Siblings of the ASTs
  // are ignored.
  // Errors are checked with ASSERTs, call in ASSERT_NO_FATAL_FAILURE.
  void check_ast_same(ast_t* expect, ast_t* actual);


  // Test methods.
  // These all build the given src code, up to the specified pass, and check
  // the resulting tree as required.
  // On successful build the resulting AST is stored in the program member with
  // the package and module members pointing to the appropriate nodes.
  // These methods all use ASSERTs, call in ASSERT_NO_FATAL_FAILURE.

  // Check that the given source compiles to the specified pass without error
  void test_compile(const char* src, const char* pass);

  // Check that the given source fails when compiled to the specified pass
  void test_error(const char* src, const char* pass);

  // Check that the 2 given sources compile to give the same AST for the first
  // package
  void test_equiv(const char* actual_src, const char* actual_pass,
    const char* expect_src, const char* expect_pass);


  // Find the type of the parameter, field or local variable with the specified
  // name in the previously built program.
  // If there are multiple matches the first found will be returned.
  // Returns: type of specified name, NULL if none found.
  ast_t* type_of(const char* name);

  // Lookup the specified name in the symbol table of the given AST node,
  // without checking any symbol tables of outer scopes.
  // Returns: the AST found in the symbol table, NULL if not found.
  ast_t* lookup_in(ast_t* ast, const char* name);

  // Lookup the type with the given name in the loaded previously package.
  // Returns: the AST of the type definition, NULL if not found.
  ast_t* lookup_type(const char* name);

  // Lookup the member with the given name in the type with the given name in
  // the loaded previously package.
  // Returns: the AST of the named definition, NULL if not found.
  ast_t* lookup_member(const char* type_name, const char* member_name);

  // Lookup the first instance of the given integer as a number literal in 
  // the previously loaded package
  ast_t* numeric_literal(uint64_t num);

private:
  const char* _builtin_src;
  const char* _first_pkg_path;

  // Attempt to compile the package with the specified name into a program.
  // Errors are checked with ASSERTs, call in ASSERT_NO_FATAL_FAILURE.
  void build_package(const char* pass, const char* src,
    const char* package_name, bool check_good, ast_t** out_package);

  // Find the type of the parameter, field or local variable with the specified
  // name in the given AST.
  // If there are multiple matches the first found will be returned.
  // Returns: type of specified name, NULL if none found.
  ast_t* type_of_within(ast_t* ast, const char* name);

  ast_t* numeric_literal_within(ast_t* ast, uint64_t num);
};


#endif
