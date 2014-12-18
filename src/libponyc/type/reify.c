#include "reify.h"
#include "subtype.h"
#include "viewpoint.h"
#include "assemble.h"
#include "alias.h"
#include "../ast/token.h"
#include <assert.h>

static bool reify_typeparamref(ast_t** astp, ast_t* typeparam, ast_t* typearg)
{
  ast_t* ast = *astp;
  assert(ast_id(ast) == TK_TYPEPARAMREF);
  ast_t* ref_name = ast_child(ast);
  ast_t* param_name = ast_child(typeparam);

  if(ast_name(ref_name) != ast_name(param_name))
    return false;

  // Keep ephemerality.
  if(ast_id(ast_childidx(ast, 2)) == TK_EPHEMERAL)
    typearg = consume_type(typearg);

  ast_replace(astp, typearg);
  return true;
}

static bool reify_one(ast_t** astp, ast_t* typeparam, ast_t* typearg)
{
  ast_t* ast = *astp;
  ast_t* type = ast_type(ast);

  if(type != NULL)
    reify_one(&type, typeparam, typearg);

  if(ast_id(ast) == TK_TYPEPARAMREF)
    return reify_typeparamref(astp, typeparam, typearg);

  ast_t* child = ast_child(ast);
  bool flatten = false;

  while(child != NULL)
  {
    flatten |= reify_one(&child, typeparam, typearg);
    child = ast_sibling(child);
  }

  // Flatten type expressions after reifying them.
  if(flatten)
  {
    switch(ast_id(ast))
    {
      case TK_UNIONTYPE:
        flatten_union(astp);
        return true;

      case TK_ISECTTYPE:
        flatten_isect(astp);
        return true;

      case TK_ARROW:
      {
        AST_GET_CHILDREN(ast, left, right);
        ast = viewpoint_type(left, right);
        ast_replace(astp, ast);
        return true;
      }

      default: {}
    }
  }

  return false;
}

ast_t* reify(ast_t* ast, ast_t* typeparams, ast_t* typeargs)
{
  assert(
    (ast_id(typeparams) == TK_TYPEPARAMS) ||
    (ast_id(typeparams) == TK_NONE)
    );
  assert(
    (ast_id(typeargs) == TK_TYPEARGS) ||
    (ast_id(typeargs) == TK_NONE)
    );

  // Duplicate the node.
  ast_t* r_ast = ast_dup(ast);

  // Iterate pairwise through the params and the args.
  ast_t* typeparam = ast_child(typeparams);
  ast_t* typearg = ast_child(typeargs);

  while((typeparam != NULL) && (typearg != NULL))
  {
    // reify the typeparam with the typearg
    reify_one(&r_ast, typeparam, typearg);
    typeparam = ast_sibling(typeparam);
    typearg = ast_sibling(typearg);
  }

  if(typearg != NULL)
  {
    ast_error(typearg, "too many type arguments");
    ast_free(r_ast);
    return NULL;
  }

  // Pick up default type arguments if they exist.
  while(typeparam != NULL)
  {
    typearg = ast_childidx(typeparam, 2);

    if(ast_id(typearg) == TK_NONE)
      break;

    // Append the default type argument to the typeargs list.
    ast_append(typeargs, typearg);
    reify_one(&r_ast, typeparam, typearg);
    typeparam = ast_sibling(typeparam);
  }

  if(typeparam != NULL)
  {
    ast_error(typeargs, "not enough type arguments");
    ast_free(r_ast);
    return NULL;
  }

  return r_ast;
}

void reify_cap_and_ephemeral(ast_t* source, ast_t** target)
{
  assert(ast_id(source) == TK_NOMINAL);
  assert(ast_id(*target) == TK_NOMINAL);
  ast_t* ast = ast_dup(*target);

  AST_GET_CHILDREN(source, pkg, id, typeargs, cap, ephemeral);
  AST_GET_CHILDREN(ast, t_pkg, t_id, t_typeargs, t_cap, t_ephemeral);

  ast_replace(&t_cap, cap);
  ast_replace(&t_ephemeral, ephemeral);

  *target = ast;
}

bool check_constraints(ast_t* typeparams, ast_t* typeargs, bool report_errors)
{
  // reify the type parameters with the typeargs
  ast_t* r_typeparams = reify(typeparams, typeparams, typeargs);

  if(r_typeparams == NULL)
    return false;

  ast_t* r_typeparam = ast_child(r_typeparams);
  ast_t* typeparam = ast_child(typeparams);
  ast_t* typearg = ast_child(typeargs);

  while(r_typeparam != NULL)
  {
    ast_t* constraint = ast_childidx(r_typeparam, 1);

    // A bound type must be a subtype of the constraint.
    if(!is_subtype(typearg, constraint))
    {
      if(report_errors)
      {
        ast_error(typearg, "type argument is outside its constraint");
        ast_error(typearg, "argument: %s", ast_print_type(typearg));
        ast_error(typeparam, "constraint: %s", ast_print_type(constraint));
      }

      ast_free_unattached(r_typeparams);
      return false;
    }

    // In addition, an alias of the bound type must be a subtype of an alias of
    // the constraint. We use a different alias operation because we are not
    // interested in ephemerality or borrowing, and we don't want trn bound
    // where a box is expected.
    ast_t* a_typearg = alias_bind(typearg);
    ast_t* a_constraint = alias_bind(constraint);

    if(!is_subtype(a_typearg, a_constraint))
    {
      if(report_errors)
      {
        ast_error(typearg, "iso and trn can only bind to themselves or tag");
        ast_error(typearg, "argument: %s", ast_print_type(typearg));
        ast_error(typeparam, "constraint: %s", ast_print_type(constraint));
      }

      ast_free_unattached(a_typearg);
      ast_free_unattached(a_constraint);
      ast_free_unattached(r_typeparams);
      return false;
    }

    ast_free_unattached(a_typearg);
    ast_free_unattached(a_constraint);

    r_typeparam = ast_sibling(r_typeparam);
    typeparam = ast_sibling(typeparam);
    typearg = ast_sibling(typearg);
  }

  assert(r_typeparam == NULL);
  assert(typearg == NULL);

  ast_free_unattached(r_typeparams);
  return true;
}
