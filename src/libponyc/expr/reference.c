#include "reference.h"
#include "literal.h"
#include "postfix.h"
#include "../type/subtype.h"
#include "../type/nominal.h"
#include "../type/assemble.h"
#include "../ds/stringtab.h"
#include <assert.h>

/**
 * Make sure the definition of something occurs before its use. This is for
 * both fields and local variable.
 */
static bool def_before_use(ast_t* def, ast_t* use, const char* name)
{
  if((ast_line(def) > ast_line(use)) ||
     ((ast_line(def) == ast_line(use)) &&
      (ast_pos(def) > ast_pos(use))))
  {
    ast_error(use, "declaration of '%s' appears after use", name);
    ast_error(def, "declaration of '%s' appears here", name);
    return false;
  }

  return true;
}

bool expr_field(ast_t* ast)
{
  ast_t* type = ast_childidx(ast, 1);
  ast_t* init = ast_sibling(type);

  if((ast_id(type) == TK_NONE) && (ast_id(init) == TK_NONE))
  {
    ast_error(ast, "field/param needs a type or an initialiser");
    return false;
  }

  if(ast_id(type) == TK_NONE)
  {
    // if no declared type, get the type from the initialiser
    ast_settype(ast, ast_type(init));
    return true;
  }

  if(ast_id(init) != TK_NONE)
  {
    // initialiser type must match declared type
    ast_t* init_type = ast_type(init);

    if(!is_subtype(ast, init_type, type))
    {
      ast_error(init,
        "field/param initialiser is not a subtype of the field/param type");
      return false;
    }
  }

  ast_settype(ast, type);
  return true;
}

bool expr_typeref(ast_t* ast)
{
  assert(ast_id(ast) == TK_TYPEREF);
  ast_t* type = ast_type(ast);

  switch(ast_id(ast_parent(ast)))
  {
    case TK_QUALIFY:
      // doesn't have to be valid yet
      break;

    case TK_DOT:
      // has to be valid
      return nominal_valid(ast, type);

    case TK_CALL:
    {
      // has to be valid
      if(!nominal_valid(ast, type))
        return false;

      // transform to a default constructor
      ast_t* dot = ast_from(ast, TK_DOT);
      ast_add(dot, ast_from_string(ast, stringtab("create")));
      ast_swap(ast, dot);
      ast_add(dot, ast);

      return expr_dot(dot);
    }

    default:
    {
      // has to be valid
      if(!nominal_valid(ast, type))
        return false;

      // transform to a default constructor
      ast_t* dot = ast_from(ast, TK_DOT);
      ast_add(dot, ast_from_string(ast, stringtab("create")));
      ast_swap(ast, dot);
      ast_add(dot, ast);

      if(!expr_dot(dot))
        return false;

      // call the default constructor with no arguments
      ast_t* tuple = ast_from(ast, TK_TUPLE);
      ast_add(tuple, ast_from(ast, TK_NONE)); // named args
      ast_add(tuple, ast_from(ast, TK_NONE)); // positional args
      ast_t* call = ast_from(ast, TK_CALL);
      ast_add(call, tuple);
      ast_swap(dot, call);
      ast_add(call, dot);

      return expr_call(call);
    }
  }

  return true;
}

bool expr_reference(ast_t* ast)
{
  // everything we reference must be in scope
  const char* name = ast_name(ast_child(ast));
  ast_t* def = ast_get(ast, name);

  if(def == NULL)
  {
    ast_error(ast, "can't find declaration of '%s'", name);
    return false;
  }

  switch(ast_id(def))
  {
    case TK_PACKAGE:
    {
      // only allowed if in a TK_DOT with a type
      if(ast_id(ast_parent(ast)) != TK_DOT)
      {
        ast_error(ast, "a package can only appear as a prefix to a type");
        return false;
      }

      ast_setid(ast, TK_PACKAGEREF);
      return true;
    }

    case TK_TYPE:
    case TK_CLASS:
    case TK_ACTOR:
    {
      // it's a type name. this may not be a valid type, since it may need
      // type arguments.
      ast_t* id = ast_child(def);
      const char* name = ast_name(id);
      ast_t* type = nominal_type(ast, NULL, name);
      ast_settype(ast, type);
      ast_setid(ast, TK_TYPEREF);

      return expr_typeref(ast);
    }

    case TK_FVAR:
    case TK_FLET:
    {
      // transform to this.f
      if(!def_before_use(def, ast, name))
        return false;

      ast_t* dot = ast_from(ast, TK_DOT);
      ast_swap(ast, dot);
      ast_add(dot, ast_child(ast));
      ast_free(ast);

      ast_t* this = ast_from(ast, TK_THIS);
      ast_add(dot, this);

      if(!expr_this(this))
        return false;

      return expr_dot(dot);
    }

    case TK_PARAM:
    {
      if(!def_before_use(def, ast, name))
        return false;

      // get the type of the parameter and attach it to our reference
      ast_settype(ast, ast_type(def));
      ast_setid(ast, TK_PARAMREF);
      return true;
    }

    case TK_NEW:
    case TK_BE:
    case TK_FUN:
    {
      // transform to this.f
      ast_t* dot = ast_from(ast, TK_DOT);
      ast_swap(ast, dot);
      ast_add(dot, ast_child(ast));
      ast_free(ast);

      ast_t* this = ast_from(ast, TK_THIS);
      ast_add(dot, this);

      if(!expr_this(this))
        return false;

      return expr_dot(dot);
    }

    case TK_ID:
    {
      if(!def_before_use(def, ast, name))
        return false;

      // TODO: separate node id for assignable and not assignable?
      // get the type of the local and attach it to our reference
      ast_settype(ast, ast_type(def));
      ast_setid(ast, TK_LOCALREF);
      return true;
    }

    default: {}
  }

  assert(0);
  return false;
}

bool expr_local(ast_t* ast)
{
  assert(ast_id(ast) == TK_IDSEQ);
  ast_t* type = ast_sibling(ast);

  if(ast_id(type) == TK_NONE)
  {
    ast_t* parent = ast_parent(ast);

    switch(ast_id(parent))
    {
      case TK_VAR:
      case TK_LET:
        if(ast_id(ast_parent(parent)) == TK_ASSIGN)
          return true;
        break;

      default: {}
    }

    ast_error(ast, "locals must specify a type or be assigned something");
    return false;
  }

  return type_for_idseq(ast, type);
}

bool expr_fun(ast_t* ast)
{
  ast_t* type = ast_childidx(ast, 4);
  ast_t* can_error = ast_sibling(type);
  ast_t* body = ast_sibling(can_error);

  if(ast_id(body) == TK_NONE)
    return true;

  ast_t* def = ast_enclosing_type(ast);
  bool is_trait = ast_id(def) == TK_TRAIT;

  // if specified, body type must match return type
  ast_t* body_type = ast_type(body);

  if(ast_id(body_type) == TK_ERROR)
  {
    ast_t* last = ast_childlast(body);
    ast_error(type, "function body always results in an error");
    ast_error(last, "function body expression is here");
    return false;
  }

  // check partial functions
  ast_t* error = ast_from(ast, TK_ERROR);
  bool ret = true;

  if(ast_id(can_error) == TK_QUESTION)
  {
    // if a partial function, check that we might actually error
    if(!is_trait && !is_subtype(ast, error, body_type))
    {
      ast_error(can_error, "function body is not partial but the function is");
      ret = false;
    }
  } else {
    // if not a partial function, check that we can't error
    if(is_subtype(ast, error, body_type))
    {
      ast_error(can_error, "function body is partial but the function is not");
      ret = false;
    }
  }

  if(ast_id(ast) == TK_FUN)
  {
    // union the result type with ERROR
    if(ast_id(can_error) == TK_QUESTION)
      type = type_union(ast, type, error);

    if(!is_subtype(ast, body_type, type))
    {
      ast_t* last = ast_childlast(body);
      ast_error(type, "function body isn't a subtype of the result type");
      ast_error(last, "function body expression is here");
      ret = false;
    }

    if(!is_trait && !is_eqtype(ast, body_type, type))
    {
      ast_t* last = ast_childlast(body);
      ast_error(type, "function body is more specific than the result type");
      ast_error(last, "function body expression is here");
      ret = false;
    }
  }

  ast_free_unattached(error);
  ast_free_unattached(type);
  return ret;
}
