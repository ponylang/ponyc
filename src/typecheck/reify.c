#include "reify.h"
#include "typechecker.h"
#include <assert.h>

static bool reify_function(ast_t* fun, ast_t* id, ast_t* typearg)
{
  // TODO:
  return true;
}

static bool reify_typedef(ast_t* type, ast_t* id, ast_t* typearg)
{
  assert(ast_id(type) == TK_TYPEDEF);
  assert(ast_id(typearg) == TK_TYPEDEF);

  // make sure typearg has nominal_def calculated in the right scope
  ast_t* sub_typearg = ast_child(typearg);

  if(ast_id(sub_typearg) == TK_NOMINAL)
    nominal_def(sub_typearg);

  ast_t* sub_type = ast_child(type);

  switch(ast_id(sub_type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      ast_t* left = ast_child(sub_type);
      ast_t* right = ast_sibling(left);
      return reify_typedef(left, id, typearg) &&
        reify_typedef(right, id, typearg);
    }

    case TK_NOMINAL:
    {
      ast_t* type_name = ast_child(sub_type);
      ast_t* type_id = ast_child(type_name);
      ast_t* typeargs = ast_sibling(type_name);

      if((ast_sibling(type_id) == NULL) && (ast_name(type_id) == ast_name(id)))
      {
        // we have only one name component and it is the id we are reifying
        // we should not have typeargs
        if(ast_id(typeargs) != TK_NONE)
          return false;

        ast_swap(type, ast_dup(typearg));
        ast_free(type);
      } else {
        ast_t* sub_typearg = ast_child(typeargs);

        while(sub_typearg != NULL)
        {
          if(!reify_typedef(sub_typearg, id, typearg))
            return false;

          sub_typearg = ast_sibling(sub_typearg);
        }
      }

      return true;
    }

    case TK_STRUCTURAL:
    {
      ast_t* fun = ast_child(sub_type);

      while(fun != NULL)
      {
        if(!reify_function(fun, id, typearg))
          return true;

        fun = ast_sibling(fun);
      }

      return true;
    }

    default: {}
  }

  assert(0);
  return false;
}

static bool reify_typeparam(ast_t* typeparams, ast_t* id, ast_t* typearg)
{
  assert(ast_id(typeparams) == TK_TYPEPARAMS);
  assert(ast_id(id) == TK_ID);
  assert(ast_id(typearg) == TK_TYPEDEF);

  ast_t* typeparam = ast_child(typeparams);

  while(typeparam != NULL)
  {
    ast_t* type_variable = ast_child(typeparam);
    ast_t* constraint = ast_sibling(type_variable);

    if((ast_id(constraint) != TK_NONE) &&
      !reify_typedef(constraint, id, typearg))
    {
      return false;
    }

    typeparam = ast_sibling(typeparam);
  }

  return true;
}

ast_t* reify_typeparams(ast_t* typeparams, ast_t* typeargs)
{
  if(ast_id(typeparams) != TK_TYPEPARAMS)
    return NULL;

  if(ast_id(typeargs) != TK_TYPEARGS)
    return NULL;

  // duplicate the typeparams
  typeparams = ast_dup(typeparams);

  // iterate pairwise through the params and the args
  ast_t* typeparam = ast_child(typeparams);
  ast_t* typearg = ast_child(typeargs);

  while((typeparam != NULL) && (typearg != NULL))
  {
    ast_t* id = ast_child(typeparam);

    if(!reify_typeparam(typeparams, id, typearg))
      break;

    typeparam = ast_sibling(typeparam);
    typearg = ast_sibling(typearg);
  }

  if(typeparam != NULL)
  {
    ast_error(typeargs, "not enough type arguments");
    ast_free(typeparams);
    return NULL;
  }

  if(typearg != NULL)
  {
    ast_error(typearg, "too many type arguments");
    ast_free(typeparams);
    return NULL;
  }

  return typeparams;
}

ast_t* reify_type(ast_t* type, ast_t* typeparams, ast_t* typeargs)
{
  assert(ast_id(type) == TK_TYPEDEF);
  assert(ast_id(typeparams) == TK_TYPEPARAMS);
  assert(ast_id(typeargs) == TK_TYPEARGS);

  // duplicate the type
  type = ast_dup(type);

  // iterate pairwise through the params and the args
  ast_t* typeparam = ast_child(typeparams);
  ast_t* typearg = ast_child(typeargs);

  while((typeparam != NULL) && (typearg != NULL))
  {
    ast_t* id = ast_child(typeparam);

    if(!reify_typedef(type, id, typearg))
      break;

    typeparam = ast_sibling(typeparam);
    typearg = ast_sibling(typearg);
  }

  if(typeparam != NULL)
  {
    ast_error(typeargs, "not enough type arguments");
    ast_free(type);
    return NULL;
  }

  if(typearg != NULL)
  {
    ast_error(typearg, "too many type arguments");
    ast_free(type);
    return NULL;
  }

  return type;
}
