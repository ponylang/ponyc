#include "assemble.h"
#include "subtype.h"
#include "lookup.h"
#include "../ast/token.h"
#include "../expr/literal.h"
#include "../pass/names.h"
#include "../pass/expr.h"
#include <assert.h>

static void append_one_to_union(ast_t* ast, ast_t* append)
{
  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    ast_t* next = ast_sibling(child);

    if(is_subtype(append, child))
    {
      // If the incoming type is a subtype of a type that is already in the
      // union, then do not bother to append it.
      return;
    } else if(is_subtype(child, append)) {
      // If a type in the union is a subtype of the incoming type, then remove
      // it from the union.
      ast_remove(child);
    }

    child = next;
  }

  ast_append(ast, append);
}

static void append_one_to_isect(ast_t* ast, ast_t* append)
{
  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    ast_t* next = ast_sibling(child);

    if(is_subtype(child, append))
    {
      // If the incoming type is a supertype of a type that is already in the
      // intersection, then do not bother to append it.
      return;
    } else if(is_subtype(append, child)) {
      // If a type in the intersection is a supertype of the incoming type,
      // then remove it from the intersection.
      ast_remove(child);
    }

    child = next;
  }

  ast_append(ast, append);
}

static void append_one_to_typeexpr(ast_t* ast, ast_t* append, bool is_union)
{
  if(is_union)
    append_one_to_union(ast, append);
  else
    append_one_to_isect(ast, append);
}

static void append_to_typeexpr(ast_t* ast, ast_t* append, bool is_union)
{
  if(ast_id(ast) == ast_id(append))
  {
    // Add each element of the typeexpr to the new typeexpr.
    ast_t* child = ast_child(append);

    while(child != NULL)
    {
      append_one_to_typeexpr(ast, child, is_union);
      child = ast_sibling(child);
    }
  } else {
    append_one_to_typeexpr(ast, append, is_union);
  }
}

static ast_t* type_typeexpr(token_id t, ast_t* l_type, ast_t* r_type)
{
  bool is_union = t == TK_UNIONTYPE;

  if(l_type == NULL)
    return r_type;

  if(r_type == NULL)
    return l_type;

  if(is_subtype(l_type, r_type))
  {
    if(is_union)
      return r_type;
    else
      return l_type;
  }

  if(is_subtype(r_type, l_type))
  {
    if(is_union)
      return l_type;
    else
      return r_type;
  }

  ast_t* type = ast_from(l_type, t);
  append_to_typeexpr(type, l_type, is_union);
  append_to_typeexpr(type, r_type, is_union);

  // If there's only one element, remove the type expression node.
  ast_t* child = ast_child(type);

  if(ast_sibling(child) == NULL)
  {
    child = ast_dup(child);
    ast_free_unattached(type);
    type = child;
  }

  return type;
}

static ast_t* type_base(ast_t* from, const char* package, const char* name)
{
  BUILD(ast, from,
    NODE(TK_NOMINAL,
      ID(package)
      ID(name)
      NONE NONE NONE));

  return ast;
}

ast_t* type_builtin(pass_opt_t* opt, ast_t* from, const char* name)
{
  ast_t* ast = type_base(from, NULL, name);

  if(!names_nominal(opt, from, &ast))
  {
    ast_error(from, "unable to validate '%s'", name);
    ast_free(ast);
    return NULL;
  }

  return ast;
}

ast_t* type_pointer_to(pass_opt_t* opt, ast_t* to)
{
  BUILD(pointer, to,
    NODE(TK_NOMINAL,
      NONE // Package
      ID("Pointer")
      NODE(TK_TYPEARGS,
        TREE(to)
        )
      NONE // Capability
      NONE // Ephemeral
      ));

  if(!names_nominal(opt, to, &pointer))
  {
    ast_error(to, "unable to create Pointer[%s]", ast_print_type(to));
    ast_free(pointer);
    return NULL;
  }

  return pointer;
}

ast_t* type_sugar(ast_t* from, const char* package, const char* name)
{
  return type_base(from, package, name);
}

ast_t* control_type_add_branch(ast_t* control_type, ast_t* branch)
{
  assert(branch != NULL);

  ast_t* branch_type = ast_type(branch);

  if(branch_type == NULL)
    return control_type;

  if(is_typecheck_error(branch_type))
    return branch_type;

  if(is_type_literal(branch_type))
  {
    // The new branch is a literal
    if(control_type == NULL)
      control_type = ast_from(branch, TK_LITERAL);

    if(ast_id(control_type) != TK_LITERAL)
    {
      // The current control type is not a literal, fix that
      ast_t* old_control = control_type;
      control_type = ast_from(branch, TK_LITERAL);
      ast_settype(control_type, old_control);
    }

    assert(ast_id(control_type) == TK_LITERAL);

    // Add a literal branch reference to the new branch
    ast_t* member = ast_from(branch, TK_LITERALBRANCH);
    ast_setdata(member, (void*)branch);
    ast_append(control_type, member);

    ast_t* branch_non_lit = ast_type(branch_type);

    if(branch_non_lit != NULL)
    {
      // The branch's literal type has a non-literal component
      ast_t* non_literal_type = ast_type(control_type);
      ast_settype(control_type, type_union(non_literal_type, branch_non_lit));
    }

    return control_type;
  }

  if(control_type != NULL && ast_id(control_type) == TK_LITERAL)
  {
    // New branch is not literal, but the control structure is
    // Add new branch type to the control structure's non-literal aspect
    ast_t* non_literal_type = ast_type(control_type);
    non_literal_type = type_union(non_literal_type, branch_type);
    ast_settype(control_type, non_literal_type);
    return control_type;
  }

  // No literals here, just union the types
  return type_union(control_type, branch_type);
}

ast_t* type_union(ast_t* l_type, ast_t* r_type)
{
  return type_typeexpr(TK_UNIONTYPE, l_type, r_type);
}

ast_t* type_isect(ast_t* l_type, ast_t* r_type)
{
  return type_typeexpr(TK_ISECTTYPE, l_type, r_type);
}

ast_t* type_for_this(typecheck_t* t, ast_t* ast, token_id cap,
  token_id ephemeral)
{
  bool make_arrow = false;

  if(cap == TK_BOX)
  {
    cap = TK_REF;
    make_arrow = true;
  }

  AST_GET_CHILDREN(t->frame->type, id, typeparams);

  BUILD(typeargs, ast, NODE(TK_NONE));

  BUILD(type, ast,
    NODE(TK_NOMINAL,
      NODE(TK_NONE)
      TREE(id)
      TREE(typeargs)
      NODE(cap)
      NODE(ephemeral)));

  if(ast_id(typeparams) == TK_TYPEPARAMS)
  {
    ast_setid(typeargs, TK_TYPEARGS);
    ast_t* typeparam = ast_child(typeparams);

    while(typeparam != NULL)
    {
      ast_t* typeparam_id = ast_child(typeparam);
      ast_t* typearg = type_sugar(ast, NULL, ast_name(typeparam_id));
      ast_append(typeargs, typearg);

      typeparam = ast_sibling(typeparam);
    }
  }

  if(make_arrow)
  {
    BUILD(arrow, ast, NODE(TK_ARROW, NODE(TK_THISTYPE) TREE(type)));
    return arrow;
  }

  return type;
}

ast_t* type_for_fun(ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, name, typeparams, params, result);
  token_id fcap = ast_id(cap);

  if(fcap == TK_NONE)
    fcap = TK_TAG;

  BUILD(fun, ast,
    NODE(TK_FUNTYPE, NODE(fcap) TREE(typeparams) TREE(params) TREE(result)));

  return fun;
}

bool flatten_union(ast_t** astp)
{
  ast_t* ast = *astp;
  ast_t* child = ast_child(ast);
  ast_t* type = NULL;

  while(child != NULL)
  {
    type = type_union(type, child);
    child = ast_sibling(child);
  }

  ast_replace(astp, type);
  return true;
}

bool flatten_isect(ast_t** astp)
{
  ast_t* ast = *astp;
  ast_t* child = ast_child(ast);
  ast_t* type = NULL;

  while(child != NULL)
  {
    type = type_isect(type, child);
    child = ast_sibling(child);
  }

  ast_replace(astp, type);
  return true;
}

ast_t* set_cap_and_ephemeral(ast_t* type, token_id cap, token_id ephemeral)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      ast_t* child = ast_child(type);
      type = ast_from(type, ast_id(type));

      while(child != NULL)
      {
        ast_append(type, set_cap_and_ephemeral(child, cap, ephemeral));
        child = ast_sibling(child);
      }

      return type;
    }

    case TK_NOMINAL:
    {
      type = ast_dup(type);
      AST_GET_CHILDREN(type, package, id, typeargs, tcap, eph);

      if(cap != TK_NONE)
        ast_setid(tcap, cap);

      ast_setid(eph, ephemeral);
      return type;
    }

    case TK_TYPEPARAMREF:
    {
      type = ast_dup(type);
      AST_GET_CHILDREN(type, id, tcap, eph);

      if(cap != TK_NONE)
        ast_setid(tcap, cap);

      ast_setid(eph, ephemeral);
      return type;
    }

    case TK_ARROW:
      // Just use the lhs of the viewpoint type.
      return set_cap_and_ephemeral(ast_childidx(type, 1), cap, ephemeral);

    default: {}
  }

  assert(0);
  return NULL;
}
