#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>
#include <pass/expr.h>
#include <ds/stringtab.h>
#include <pass/pass.h>
#include <pkg/package.h>

#include "util.h"

class ExprLiteralTest: public testing::Test
{};


/*
TEST(ExprLiteralTest, Int)
{
  const char* tree = "(3:scope)";
  const char* uint_def = "(primitive x (id UIntLiteral) x x x x)";
  const char* expect = "(3:scope [(nominal x (id UIntLiteral) x val x)])";

  ast_t* module = parse_test_module(tree);
  add_to_symbtab(module, "UIntLiteral", uint_def);

  ASSERT_EQ(AST_OK, pass_expr(&module));
  ASSERT_NO_FATAL_FAILURE(check_tree(expect, module));

  ast_free(module);
}
*/


/*
case TK_NOMINAL:
if(!expr_nominal(astp))
return AST_FATAL;
break;

case TK_FVAR:
case TK_FLET:
case TK_PARAM:
if(!expr_field(ast))
return AST_FATAL;
break;

case TK_NEW:
// TODO: check that the object is fully initialised
if(!expr_fun(ast))
return AST_FATAL;
break;

case TK_BE:
case TK_FUN:
if(!expr_fun(ast))
return AST_FATAL;
break;

case TK_SEQ:
if(!expr_seq(ast))
return AST_FATAL;
break;

case TK_VAR:
case TK_LET:
if(!expr_local(ast))
return AST_FATAL;
break;

case TK_IDSEQ:
if(!expr_idseq(ast))
return AST_FATAL;
break;

case TK_BREAK:
if(!expr_break(ast))
return AST_FATAL;
break;

case TK_CONTINUE:
if(!expr_continue(ast))
return AST_FATAL;
break;

case TK_RETURN:
if(!expr_return(ast))
return AST_FATAL;
break;

case TK_DOT:
if(!expr_dot(ast))
return AST_FATAL;
break;

case TK_QUALIFY:
if(!expr_qualify(ast))
return AST_FATAL;
break;

case TK_CALL:
if(!expr_call(ast))
return AST_FATAL;
break;

case TK_IF:
if(!expr_if(ast))
return AST_FATAL;
break;

case TK_WHILE:
if(!expr_while(ast))
return AST_FATAL;
break;

case TK_REPEAT:
if(!expr_repeat(ast))
return AST_FATAL;
break;

case TK_TRY:
if(!expr_try(ast))
return AST_FATAL;
break;

case TK_MATCH:
if(!expr_match(ast))
return AST_FATAL;
break;

case TK_CASES:
if(!expr_cases(ast))
return AST_FATAL;
break;

case TK_CASE:
if(!expr_case(ast))
return AST_FATAL;
break;

case TK_AS:
if(!expr_as(ast))
return AST_FATAL;
break;

case TK_TUPLE:
if(!expr_tuple(ast))
return AST_FATAL;
break;

case TK_ARRAY:
// TODO: determine our type by looking at every expr in the array
ast_error(ast, "not implemented (array)");
return AST_FATAL;

case TK_OBJECT:
// TODO: create a structural type for the object
// TODO: make sure it fulfills any traits it claims to have
ast_error(ast, "not implemented (object)");
return AST_FATAL;

case TK_REFERENCE:
if(!expr_reference(ast))
return AST_FATAL;
break;

case TK_THIS:
if(!expr_this(ast))
return AST_FATAL;
break;

case TK_INT:
if(!expr_literal(ast, "UIntLiteral"))
return AST_FATAL;
break;

case TK_FLOAT:
if(!expr_literal(ast, "FloatLiteral"))
return AST_FATAL;
break;

case TK_STRING:
if(!expr_literal(ast, "String"))
return AST_FATAL;
break;

case TK_ERROR:
if(!expr_error(ast))
return AST_FATAL;
break;

case TK_COMPILER_INTRINSIC:
if(!expr_compiler_intrinsic(ast))
return AST_FATAL;
break;

case TK_POSITIONALARGS:
case TK_NAMEDARGS:
case TK_NAMEDARG:
ast_inheriterror(ast);
break;
*/
