#include "assemble.h"
#include "nominal.h"
#include "subtype.h"
#include "lookup.h"
#include <assert.h>

ast_t* type_builtin(ast_t* ast, const char* name)
{
  ast_t* type = ast_type(ast);
  ast_t* builtin = nominal_builtin(ast, name);
  bool ok = is_subtype(ast, type, builtin);
  ast_free(builtin);

  if(!ok)
    return NULL;

  return type;
}

ast_t* type_bool(ast_t* ast)
{
  return type_builtin(ast, "Bool");
}

ast_t* type_int(ast_t* ast)
{
  return type_builtin(ast, "Integer");
}

ast_t* type_int_or_bool(ast_t* ast)
{
  ast_t* type = type_bool(ast);

  if(type == NULL)
    type = type_int(ast);

  if(type == NULL)
  {
    ast_error(ast, "expected Bool or an integer type");
    return NULL;
  }

  return type;
}

ast_t* type_arithmetic(ast_t* ast)
{
  return type_builtin(ast, "Arithmetic");
}

ast_t* type_super(ast_t* scope, ast_t* l_type, ast_t* r_type)
{
  if((l_type == NULL) || (r_type == NULL))
    return NULL;

  if(is_subtype(scope, l_type, r_type))
    return r_type;

  if(is_subtype(scope, r_type, l_type))
    return l_type;

  return NULL;
}

ast_t* type_union(ast_t* ast, ast_t* l_type, ast_t* r_type)
{
  if(l_type == NULL)
    return r_type;

  if(r_type == NULL)
    return l_type;

  ast_t* super = type_super(ast, l_type, r_type);

  if(super != NULL)
    return super;

  ast_t* type = ast_from(ast, TK_UNIONTYPE);
  ast_add(type, r_type);
  ast_add(type, l_type);

  return type;
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
  ast_add(nominal, ast_from(ast, cap));

  if(ast_id(typeparams) == TK_TYPEPARAMS)
  {
    ast_t* typeparam = ast_child(typeparams);
    ast_t* typeargs = ast_from(ast, TK_TYPEARGS);
    ast_add(nominal, typeargs);

    while(typeparam != NULL)
    {
      ast_t* typeparam_id = ast_child(typeparam);
      ast_t* typearg = nominal_sugar(ast, NULL, ast_name(typeparam_id));
      ast_append(typeargs, typearg);

      typeparam = ast_sibling(typeparam);
    }
  } else {
    ast_add(nominal, ast_from(ast, TK_NONE)); // empty typeargs
  }

  ast_add(nominal, ast_from_string(ast, name));
  ast_add(nominal, ast_from(ast, TK_NONE));
  return nominal;
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
    ast_t* t = lookup_tuple(type, index);

    if(t == NULL)
    {
      ast_error(type, "not enough types specified");
      return false;
    }

    ast_settype(id, t);
    id = ast_sibling(id);
    index++;
  }

  if(lookup_tuple(type, index) != NULL)
  {
    ast_error(type, "too many types specified");
    return false;
  }

  return true;
}
