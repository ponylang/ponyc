#include "array.h"
#include "reference.h"
#include "postfix.h"
#include "../pass/names.h"
#include "../type/assemble.h"

bool expr_array(typecheck_t* t, ast_t** astp)
{
  ast_t* ast = *astp;
  size_t size = ast_childcount(ast);
  ast_t* child = ast_child(ast);
  ast_t* type = NULL;

  while(child != NULL)
  {
    ast_t* c_type = ast_type(child);

    if(c_type == NULL)
    {
      ast_error(child,
        "can't use an expression without a value in an array constructor");
      return false;
    }

    type = type_union(type, c_type);
    child = ast_sibling(child);
  }

  BUILD(ref, ast, NODE(TK_REFERENCE, ID("Array")));

  BUILD(qualify, ast,
    NODE(TK_QUALIFY,
      TREE(ref)
      NODE(TK_TYPEARGS, TREE(type))));

  BUILD(dot, ast, NODE(TK_DOT, TREE(qualify) ID("create")));

  ast_t* size_arg = ast_from_int(ast, size);
  ast_settype(size_arg, type_builtin(ast, "U64"));

  BUILD(call, ast,
    NODE(TK_CALL,
      NODE(TK_POSITIONALARGS, TREE(size_arg))
      NONE
      TREE(dot)));

  if(!expr_reference(t, ref) ||
    !expr_qualify(t, qualify) ||
    !expr_dot(t, dot) ||
    !expr_call(t, call)
    )
    return false;

  ast_swap(ast, call);
  *astp = call;

  child = ast_child(ast);

  while(child != NULL)
  {
    BUILD(append_dot, ast, NODE(TK_DOT, TREE(*astp) ID("append")));

    BUILD(append, ast,
      NODE(TK_CALL,
        NODE(TK_POSITIONALARGS, TREE(child))
        NONE
        TREE(append_dot)));

    ast_replace(astp, append);

    if(!expr_dot(t, append_dot) ||
      !expr_call(t, append)
      )
      return false;

    child = ast_sibling(child);
  }

  ast_free_unattached(ast);
  return true;
}
