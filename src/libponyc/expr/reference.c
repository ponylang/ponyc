#include "reference.h"
#include "literal.h"
#include "postfix.h"
#include "../ast/token.h"
#include "../type/subtype.h"
#include "../type/assemble.h"
#include "../type/alias.h"
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
    ast_settype(ast, alias(ast_type(init)));
    return true;
  }

  if(ast_id(init) != TK_NONE)
  {
    // initialiser type must match declared type
    ast_t* init_type = alias(ast_type(init));
    bool ok = is_subtype(init_type, type);
    ast_free_unattached(init_type);

    if(!ok)
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
      return expr_nominal(&type);

    case TK_CALL:
    {
      // has to be valid
      if(!expr_nominal(&type))
        return false;

      // transform to a default constructor
      ast_t* dot = ast_from(ast, TK_DOT);
      ast_add(dot, ast_from_string(ast, "create"));
      ast_swap(ast, dot);
      ast_add(dot, ast);

      return expr_dot(dot);
    }

    default:
    {
      // has to be valid
      if(!expr_nominal(&type))
        return false;

      // transform to a default constructor
      ast_t* dot = ast_from(ast, TK_DOT);
      ast_add(dot, ast_from_string(ast, "create"));
      ast_swap(ast, dot);
      ast_add(dot, ast);

      if(!expr_dot(dot))
        return false;

      // call the default constructor with no arguments
      ast_t* call = ast_from(ast, TK_CALL);
      ast_add(call, ast_from(ast, TK_NONE)); // named args
      ast_add(call, ast_from(ast, TK_NONE)); // positional args
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

    case TK_TRAIT:
    {
      ast_error(ast, "can't use a trait in an expression");
      return false;
    }

    case TK_TYPEPARAM: // TODO: is this ok or even needed?
    case TK_DATA:
    case TK_CLASS:
    case TK_ACTOR:
    {
      // it's a type name. this may not be a valid type, since it may need
      // type arguments.
      ast_t* id = ast_child(def);
      const char* name = ast_name(id);
      ast_t* type = type_sugar(ast, NULL, name);
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

      ast_t* idseq = ast_parent(def);
      ast_t* var = ast_parent(idseq);
      assert(ast_id(idseq) == TK_IDSEQ);

      switch(ast_id(var))
      {
        case TK_AS:
        case TK_VAR:
          ast_setid(ast, TK_VARREF);
          break;

        case TK_LET:
          ast_setid(ast, TK_LETREF);
          break;

        default:
          assert(0);
          return false;
      }

      // get the type of the local and attach it to our reference
      ast_settype(ast, ast_type(def));
      return true;
    }

    default: {}
  }

  assert(0);
  return false;
}

bool expr_local(ast_t* ast)
{
  ast_t* idseq = ast_child(ast);
  ast_settype(ast, ast_type(idseq));
  return true;
}

bool expr_idseq(ast_t* ast)
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

  ast_settype(ast, type);
  return type_for_idseq(ast, type);
}
