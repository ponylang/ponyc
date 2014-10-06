#include "assemble.h"
#include "assemble.h"
#include "subtype.h"
#include "lookup.h"
#include "../ast/token.h"
#include "../pass/names.h"
#include <assert.h>

static void append_one_to_typeexpr(ast_t* ast, ast_t* append, bool is_union)
{
  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    // don't append if we're already in the type expression
    if(is_union)
    {
      if(is_subtype(append, child))
        return;
    } else {
      if(is_subtype(child, append))
        return;
    }

    child = ast_sibling(child);
  }

  ast_append(ast, append);
}

static void append_to_typeexpr(ast_t* ast, ast_t* append, bool is_union)
{
  if(ast_id(ast) == ast_id(append))
  {
    // add each element of the typeexpr to the new typeexpr
    ast_t* child = ast_child(append);

    while(child != NULL)
    {
      append_one_to_typeexpr(ast, append, is_union);
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

  return type;
}

static ast_t* type_base(ast_t* from, const char* package, const char* name)
{
  ast_t* ast = ast_from(from, TK_NOMINAL);
  ast_add(ast, ast_from(from, TK_NONE)); // ephemerality
  ast_add(ast, ast_from(from, TK_NONE)); // capability
  ast_add(ast, ast_from(from, TK_NONE)); // typeargs
  ast_add(ast, ast_from_string(from, name)); // name
  ast_add(ast, ast_from_string(from, package));

  return ast;
}

ast_t* type_builtin(ast_t* from, const char* name)
{
  ast_t* ast = type_base(from, NULL, name);

  if(!names_nominal(from, &ast))
  {
    ast_error(from, "unable to validate '%s'", name);
    ast_free(ast);
    return NULL;
  }

  return ast;
}

ast_t* type_sugar(ast_t* from, const char* package, const char* name)
{
  return type_base(from, package, name);
}

ast_t* type_union(ast_t* l_type, ast_t* r_type)
{
  return type_typeexpr(TK_UNIONTYPE, l_type, r_type);
}

ast_t* type_isect(ast_t* l_type, ast_t* r_type)
{
  return type_typeexpr(TK_UNIONTYPE, l_type, r_type);
}

ast_t* type_for_this(ast_t* ast, token_id cap, bool ephemeral)
{
  ast_t* def = ast_enclosing_type(ast);
  assert(def != NULL);

  ast_t* id = ast_child(def);
  ast_t* typeparams = ast_sibling(id);
  const char* name = ast_name(id);

  ast_t* nominal = ast_from(ast, TK_NOMINAL);
  ast_add(nominal, ast_from(ast, ephemeral ? TK_HAT : TK_NONE));

  if(cap == TK_BOX)
    ast_add(nominal, ast_from(ast, TK_REF));
  else
    ast_add(nominal, ast_from(ast, cap));

  if(ast_id(typeparams) == TK_TYPEPARAMS)
  {
    ast_t* typeparam = ast_child(typeparams);
    ast_t* typeargs = ast_from(ast, TK_TYPEARGS);
    ast_add(nominal, typeargs);

    while(typeparam != NULL)
    {
      ast_t* typeparam_id = ast_child(typeparam);
      ast_t* typearg = type_sugar(ast, NULL, ast_name(typeparam_id));
      ast_append(typeargs, typearg);

      typeparam = ast_sibling(typeparam);
    }
  } else {
    ast_add(nominal, ast_from(ast, TK_NONE)); // empty typeargs
  }

  ast_add(nominal, ast_from_string(ast, name));
  ast_add(nominal, ast_from(ast, TK_NONE));

  if(cap != TK_BOX)
    return nominal;

  ast_t* arrow = ast_from(ast, TK_ARROW);
  ast_add(arrow, nominal);
  ast_add(arrow, ast_from(ast, TK_THISTYPE));
  return arrow;
}

ast_t* type_for_fun(ast_t* ast)
{
  ast_t* cap = ast_child(ast);
  ast_t* name = ast_sibling(cap);
  ast_t* typeparams = ast_sibling(name);
  ast_t* params = ast_sibling(typeparams);
  ast_t* result = ast_sibling(params);

  ast_t* fun = ast_from(ast, TK_FUNTYPE);
  ast_add(fun, result);
  ast_add(fun, params);
  ast_add(fun, typeparams);
  ast_add(fun, cap);

  return fun;
}

bool type_for_idseq(ast_t* idseq, ast_t* type)
{
  assert(ast_id(idseq) == TK_IDSEQ);
  ast_t* id = ast_child(idseq);

  if(ast_sibling(id) == NULL)
  {
    ast_settype(id, type);
    return true;
  }

  if(ast_id(type) != TK_TUPLETYPE)
  {
    ast_error(type, "must specify a tuple type for multiple identifiers");
    return false;
  }

  int index = 0;

  while(id != NULL)
  {
    ast_t* t = ast_childidx(type, index);

    if(t == NULL)
    {
      ast_error(type, "not enough types specified");
      return false;
    }

    ast_settype(id, t);
    id = ast_sibling(id);
    index++;
  }

  if(ast_childidx(type, index) != NULL)
  {
    ast_error(type, "too many types specified");
    return false;
  }

  return true;
}
