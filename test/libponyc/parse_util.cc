#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>
#include <ast/builder.h>
#include <pkg/package.h>
#include <pass/pass.h>

#include "parse_util.h"


static const char* builtin =
"primitive U32 "
"primitive None";


void parse_test_good(const char* src, const char* expect)
{
  package_add_magic("prog", src);
  package_add_magic("builtin", builtin);
  limit_passes("parsefix");

  ast_t* actual_ast;
  DO(load_test_program("prog", &actual_ast));

  if(expect != NULL)
  {
    builder_t* builder;
    ast_t* expect_ast;
    DO(build_ast_from_string(expect, &expect_ast, &builder));

    free_errors();
    bool r = build_compare_asts(expect_ast, actual_ast);

    if(!r)
    {
      printf("Expected:\n");
      ast_print(expect_ast);
      printf("Got:\n");
      ast_print(actual_ast);
      print_errors();
    }

    ASSERT_TRUE(r);

    builder_free(builder);
  }

  ast_free(actual_ast);
}


void parse_test_bad(const char* src)
{
  package_add_magic("prog", src);
  package_add_magic("builtin", builtin);
  limit_passes("parsefix");
  package_suppress_build_message();

  pass_opt_t opt;
  pass_opt_init(&opt);
  ASSERT_EQ((void*)NULL, program_load(stringtab("prog"), &opt));
  pass_opt_done(&opt);
}
