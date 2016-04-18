#include "assemble.h"
#include "subtype.h"
#include "cap.h"
#include "../ast/token.h"
#include "../ast/astbuild.h"
#include "../expr/literal.h"
#include "../pass/names.h"
#include "../pass/flatten.h"
#include "../pass/expr.h"
#include <assert.h>

static void append_one_to_union(pass_opt_t* opt, ast_t* ast, ast_t* append)
{
  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    ast_t* next = ast_sibling(child);

    if(is_subtype(append, child, NULL, opt))
    {
      // If the incoming type is a subtype of a type that is already in the
      // union, then do not bother to append it.
      return;
    } else if(is_subtype(child, append, NULL, opt)) {
      // If a type in the union is a subtype of the incoming type, then remove
      // it from the union.
      ast_remove(child);
    }

    child = next;
  }

  ast_append(ast, append);
}

static void append_one_to_isect(pass_opt_t* opt, ast_t* ast, ast_t* append)
{
  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    ast_t* next = ast_sibling(child);

    if(is_subtype(child, append, NULL, opt))
    {
      // If the incoming type is a supertype of a type that is already in the
      // intersection, then do not bother to append it.
      return;
    } else if(is_subtype(append, child, NULL, opt)) {
      // If a type in the intersection is a supertype of the incoming type,
      // then remove it from the intersection.
      ast_remove(child);
    }

    child = next;
  }

  ast_append(ast, append);
}

static void append_one_to_typeexpr(pass_opt_t* opt, ast_t* ast, ast_t* append,
  bool is_union)
{
  if(is_union)
    append_one_to_union(opt, ast, append);
  else
    append_one_to_isect(opt, ast, append);
}

static void append_to_typeexpr(pass_opt_t* opt, ast_t* ast, ast_t* append,
  bool is_union)
{
  if(ast_id(ast) == ast_id(append))
  {
    // Add each element of the typeexpr to the new typeexpr.
    ast_t* child = ast_child(append);

    while(child != NULL)
    {
      append_one_to_typeexpr(opt, ast, child, is_union);
      child = ast_sibling(child);
    }
  } else {
    append_one_to_typeexpr(opt, ast, append, is_union);
  }
}

static ast_t* type_typeexpr(pass_opt_t* opt, token_id t, ast_t* l_type,
  ast_t* r_type)
{
  bool is_union = t == TK_UNIONTYPE;

  if(l_type == NULL)
    return r_type;

  if(r_type == NULL)
    return l_type;

  if(is_subtype(l_type, r_type, NULL, opt))
  {
    if(is_union)
      return r_type;
    else
      return l_type;
  }

  if(is_subtype(r_type, l_type, NULL, opt))
  {
    if(is_union)
      return l_type;
    else
      return r_type;
  }

  ast_t* type = ast_from(l_type, t);
  append_to_typeexpr(opt, type, l_type, is_union);
  append_to_typeexpr(opt, type, r_type, is_union);

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

static ast_t* type_base(ast_t* from, const char* package, const char* name,
  ast_t* typeargs)
{
  if(typeargs == NULL)
    typeargs = ast_from(from, TK_NONE);

  BUILD(ast, from,
    NODE(TK_NOMINAL,
      ID(package)
      ID(name)
      TREE(typeargs)
      NONE
      NONE));

  return ast;
}

ast_t* type_builtin(pass_opt_t* opt, ast_t* from, const char* name)
{
  return type_builtin_args(opt, from, name, NULL);
}

ast_t* type_builtin_args(pass_opt_t* opt, ast_t* from, const char* name,
  ast_t* typeargs)
{
  ast_t* ast = type_base(from, NULL, name, typeargs);

  if(!names_nominal(opt, from, &ast, false))
  {
    ast_error(opt->check.errors, from, "unable to validate '%s'", name);
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

  if(!names_nominal(opt, to, &pointer, false))
  {
    ast_error(opt->check.errors, to, "unable to create Pointer[%s]",
      ast_print_type(to));
    ast_free(pointer);
    return NULL;
  }

  return pointer;
}

ast_t* type_sugar(ast_t* from, const char* package, const char* name)
{
  return type_base(from, package, name, NULL);
}

ast_t* control_type_add_branch(pass_opt_t* opt, ast_t* control_type,
  ast_t* branch)
{
  assert(branch != NULL);

  ast_t* branch_type = ast_type(branch);

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
      ast_settype(control_type, type_union(opt, non_literal_type,
        branch_non_lit));
    }

    return control_type;
  }

  if(control_type != NULL && ast_id(control_type) == TK_LITERAL)
  {
    // New branch is not literal, but the control structure is
    // Add new branch type to the control structure's non-literal aspect
    ast_t* non_literal_type = ast_type(control_type);
    non_literal_type = type_union(opt, non_literal_type, branch_type);
    ast_settype(control_type, non_literal_type);
    return control_type;
  }

  // No literals here, just union the types
  return type_union(opt, control_type, branch_type);
}

ast_t* type_union(pass_opt_t* opt, ast_t* l_type, ast_t* r_type)
{
  return type_typeexpr(opt, TK_UNIONTYPE, l_type, r_type);
}

ast_t* type_isect(pass_opt_t* opt, ast_t* l_type, ast_t* r_type)
{
  return type_typeexpr(opt, TK_ISECTTYPE, l_type, r_type);
}

ast_t* type_for_this(pass_opt_t* opt, ast_t* ast, token_id cap,
  token_id ephemeral, bool defs)
{
  typecheck_t* t = &opt->check;
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

      if(defs)
      {
        names_nominal(opt, ast, &typearg, false);

        if(ast_id(typearg) == TK_TYPEPARAMREF)
          flatten_typeparamref(opt, typearg);
      }

      typeparam = ast_sibling(typeparam);
    }
  }

  if(defs)
    names_nominal(opt, ast, &type, false);

  return type;
}

ast_t* type_for_fun(ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, name, typeparams, params, result);
  token_id fcap = ast_id(cap);

  if(fcap == TK_NONE)
    fcap = TK_TAG;

  // The params may already have types attached. If we build the function type
  // directly from those we'll get nested types which can mess things up. To
  // avoid this make a clean version of the params without types.
  ast_t* clean_params = ast_dup(params);

  for(ast_t* p = ast_child(clean_params); p != NULL; p = ast_sibling(p))
    ast_settype(p, NULL);

  BUILD(fun, ast,
    NODE(TK_FUNTYPE,
      NODE(fcap) TREE(typeparams) TREE(clean_params) TREE(result)));

  return fun;
}

ast_t* type_isect_fun(pass_opt_t* opt, ast_t* a, ast_t* b)
{
  token_id ta = ast_id(a);
  token_id tb = ast_id(b);

  if(((ta == TK_NEW) || (tb == TK_NEW)) && (ta != tb))
    return NULL;

  AST_GET_CHILDREN(a, a_cap, a_id, a_typeparams, a_params, a_result, a_throw);
  AST_GET_CHILDREN(b, b_cap, b_id, b_typeparams, b_params, b_result, b_throw);

  // Must have the same name.
  if(ast_name(a_id) != ast_name(b_id))
    return NULL;

  // Must have the same number of type parameters and parameters.
  if((ast_childcount(a_typeparams) != ast_childcount(b_typeparams)) ||
    (ast_childcount(a_params) != ast_childcount(b_params)))
    return NULL;

  // Contravariant receiver cap.
  token_id tcap;
  token_id a_tcap = ast_id(a_cap);
  token_id b_tcap = ast_id(b_cap);

  if(is_cap_sub_cap(b_tcap, TK_NONE, a_tcap, TK_NONE))
    tcap = a_tcap;
  else if(is_cap_sub_cap(a_tcap, TK_NONE, b_tcap, TK_NONE))
    tcap = b_tcap;
  else
    tcap = TK_BOX;

  // Result is the intersection of the results.
  ast_t* result = type_isect(opt, a_result, b_result);

  // Covariant throws.
  token_id throws;

  if((ast_id(a_throw) == TK_NONE) || (ast_id(b_throw) == TK_NONE))
    throws = TK_NONE;
  else
    throws = TK_QUESTION;

  BUILD(fun, a,
    NODE(tcap)
    TREE(a_id)
    NODE(TK_TYPEPARAMS)
    NODE(TK_PARAMS)
    TREE(result)
    NODE(throws)
    );

  // TODO: union typeparams and params
  // handling typeparam names is tricky
  return fun;
}

ast_t* set_cap_and_ephemeral(ast_t* type, token_id cap, token_id ephemeral)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
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
