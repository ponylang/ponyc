#include "lambda.h"
#include "../ast/astbuild.h"
#include "../pass/pass.h"
#include "../type/sanitise.h"
#include <assert.h>


// Process the given capture and create the AST for the corresponding field.
// Returns the create field AST, which must be freed by the caller.
// Returns NULL on error.
static ast_t* make_capture_field(ast_t* capture)
{
  assert(capture != NULL);

  AST_GET_CHILDREN(capture, id_node, type, value);
  const char* name = ast_name(id_node);

  // There are 3 varieties of capture:
  // x -> capture variable x, type from defn of x
  // x = y -> capture expression y, type inferred from expression type
  // x: T = y -> capture expression y, type T

  if(ast_id(value) == TK_NONE)
  {
    // Variable capture
    assert(ast_id(type) == TK_NONE);

    ast_t* def = ast_get(capture, name, NULL);

    if(def == NULL)
    {
      ast_error(id_node, "cannot capture \"%s\", variable not defined",
        name);
      return NULL;
    }

    token_id def_id = ast_id(def);

    if(def_id != TK_ID && def_id != TK_FVAR && def_id != TK_FLET &&
      def_id != TK_PARAM)
    {
      ast_error(id_node, "cannot capture \"%s\", can only capture fields, "
        "parameters and local variables", name);
      return NULL;
    }

    BUILD(capture_rhs, id_node, NODE(TK_REFERENCE, ID(name)));

    type = ast_type(def);
    value = capture_rhs;
  }
  else
  {
    // Expression capture
    if(ast_id(type) == TK_NONE)
    {
      // No type specified, use type of the captured expression
      type = ast_type(value);
    }
  }

  type = sanitise_type(type);

  BUILD(field, id_node,
    NODE(TK_FLET,
      TREE(id_node)
      TREE(type)
      TREE(value)
      NONE));  // Delegate type

  return field;
}


bool expr_lambda(pass_opt_t* opt, ast_t** astp)
{
  assert(astp != NULL);
  ast_t* ast = *astp;
  assert(ast != NULL);

  AST_GET_CHILDREN(ast, cap, t_params, params, captures, ret_type, raises,
    body);

  ast_t* members = ast_from(ast, TK_MEMBERS);
  ast_t* last_member = NULL;
  bool failed = false;

  // Process captures
  for(ast_t* p = ast_child(captures); p != NULL; p = ast_sibling(p))
  {
    ast_t* field = make_capture_field(p);

    if(field != NULL)
      ast_list_append(members, &last_member, field);
    else  // An error occurred, just keep going to potentially find more errors
      failed = true;
  }

  if(failed)
  {
    ast_free(members);
    return false;
  }

  // Stop the various elements being marked as preserve
  ast_clearflag(t_params, AST_FLAG_PRESERVE);
  ast_clearflag(params, AST_FLAG_PRESERVE);
  ast_clearflag(ret_type, AST_FLAG_PRESERVE);
  ast_clearflag(body, AST_FLAG_PRESERVE);

  // Make the apply function
  BUILD(apply, ast,
    NODE(TK_FUN, AST_SCOPE
      NONE  // Capability
      ID("apply")
      TREE(t_params)
      TREE(params)
      TREE(ret_type)
      TREE(raises)
      TREE(body)
      NONE));  // Doc string

  ast_list_append(members, &last_member, apply);

  // Replace lambda with object literal
  REPLACE(astp,
    NODE(TK_OBJECT,
      TREE(cap);
      NONE  // Provides list
      TREE(members)));

  // Catch up passes
  return ast_passes_subtree(astp, opt, PASS_EXPR);
}
