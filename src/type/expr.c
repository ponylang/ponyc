#include "expr.h"
#include "valid.h"
#include "subtype.h"
#include "typechecker.h"
#include "../ds/stringtab.h"
#include <assert.h>

ast_t* get_def(ast_t* ast, const char* name)
{
  ast_t* def = ast_get(ast, name);

  if(def == NULL)
  {
    ast_error(ast, "can't find declaration of '%s'", name);
    return NULL;
  }

  return def;
}

bool def_before_use(ast_t* def, ast_t* use, const char* name)
{
  switch(ast_id(def))
  {
    case TK_FVAR:
    case TK_FLET:
    case TK_VAR:
    case TK_LET:
    {
      // fields and locals must be declared before they are used
      if((ast_line(def) > ast_line(use)) ||
         ((ast_line(def) == ast_line(use)) &&
          (ast_pos(def) > ast_pos(use))))
      {
        ast_error(use, "declaration of '%s' appears after use", name);
        ast_error(def, "declaration of '%s' appears here", name);
        return false;
      }
      break;
    }

    default: {}
  }

  return true;
}

static bool expr_field(ast_t* ast)
{
  ast_t* type = ast_childidx(ast, 1);
  ast_t* init = ast_sibling(type);

  if((ast_id(type) == TK_NONE) && (ast_id(init) == TK_NONE))
  {
    ast_error(ast, "field needs a type or an initialiser");
    return false;
  }

  if(ast_id(type) == TK_NONE)
  {
    // if no declared type, get the type from the initializer
    ast_t* init_type = ast_type(init);
    ast_swap(type, ast_dup(init_type));
    ast_free(type);
  } else if(ast_id(init) != TK_NONE) {
    // initializer type must match declared type
    ast_t* init_type = ast_type(init);

    if(!is_subtype(init_type, type))
    {
      ast_error(init,
        "field initialiser is not a subtype of the field type");
      return false;
    }
  }

  return true;
}

static ast_t* typedef_for_name(ast_t* ast, const char* name)
{
  size_t line = ast_line(ast);
  size_t pos = ast_line(ast);

  ast_t* type_def = ast_new(TK_TYPEDEF, line, pos, NULL);

  ast_t* nominal = ast_new(TK_NOMINAL, line, pos, NULL);
  ast_add(type_def, nominal);

  ast_add(nominal, ast_new(TK_NONE, line, pos, NULL));
  ast_add(nominal, ast_newid(ast, name));
  ast_add(nominal, ast_new(TK_NONE, line, pos, NULL));

  if(!valid_nominal(ast, nominal))
  {
    ast_error(nominal, "couldn't create valid type for '%s'", name);
    ast_free(type_def);
    return NULL;
  }

  return type_def;
}

static bool typedef_for_builtin(ast_t* ast, const char* name)
{
  ast_t* type_def = typedef_for_name(ast, stringtab(name));

  if(type_def == NULL)
    return false;

  ast_append(ast, type_def);
  return true;
}

static ast_t* typedef_for_id(ast_t* ast, ast_t* id)
{
  assert(ast_id(id) == TK_ID);
  return typedef_for_name(ast, ast_name(id));
}

static bool typedef_for_this(ast_t* ast)
{
  size_t line = ast_line(ast);
  size_t pos = ast_line(ast);
  ast_t* def = ast_nearest(ast, TK_TRAIT);

  if(def == NULL)
    def = ast_nearest(ast, TK_CLASS);

  if(def == NULL)
    def = ast_nearest(ast, TK_ACTOR);

  assert(def != NULL);
  ast_t* id = ast_child(def);
  ast_t* typeparams = ast_sibling(id);
  const char* name = ast_name(id);

  ast_t* type_def = ast_new(TK_TYPEDEF, line, pos, NULL);

  ast_t* nominal = ast_new(TK_NOMINAL, line, pos, NULL);
  ast_add(type_def, nominal);

  if(ast_id(typeparams) == TK_TYPEPARAMS)
  {
    ast_t* typeparam = ast_child(typeparams);
    ast_t* typeargs = ast_new(TK_TYPEARGS, line, pos, NULL);
    ast_add(nominal, typeargs);

    while(typeparam != NULL)
    {
      ast_t* typeparam_id = ast_child(typeparam);
      ast_t* typearg = typedef_for_id(ast, typeparam_id);
      ast_append(typeargs, typearg);

      typeparam = ast_sibling(typeparam);
    }
  } else {
    ast_add(nominal, ast_new(TK_NONE, line, pos, NULL));
  }

  ast_add(nominal, ast_newid(ast, name));
  ast_add(nominal, ast_new(TK_NONE, line, pos, NULL));

  if(!valid_nominal(ast, nominal))
  {
    ast_error(nominal, "couldn't create valid type for '%s'", name);
    ast_free(type_def);
    return false;
  }

  ast_append(ast, type_def);
  return true;
}

static bool typedef_for_reference(ast_t* ast)
{
  // everything we reference must be in scope
  const char* name = ast_name(ast_child(ast));
  ast_t* def = get_def(ast, name);

  if(def == NULL)
    return false;

  if(!def_before_use(def, ast, name))
    return false;

  // TODO: get the type?
  switch(ast_id(def))
  {
    case TK_PACKAGE:
    case TK_TYPE:
    case TK_CLASS:
    case TK_ACTOR:
    case TK_FVAR:
    case TK_FLET:
    case TK_NEW:
    case TK_BE:
    case TK_FUN:
    case TK_PARAM:
    case TK_IDSEQ:
      break;

    default:
      assert(0);
  }

  return true;
}

/**
 * This checks the type of all expressions.
 */
bool type_expr(ast_t* ast, int verbose)
{
  switch(ast_id(ast))
  {
    case TK_FVAR:
    case TK_FLET:
      return expr_field(ast);

    case TK_NEW:
      // TODO: check that the object is fully initialised
      // TODO: if ?, check that we might actually error
      // TODO: if not ?, check that we can't error
      // TODO: can only return this
      break;

    case TK_BE:
      // TODO: can only return None
      // TODO: check that we can't error
      break;

    case TK_FUN:
      // TODO: body type must match return types
      // TODO: if ?, check that we might actually error
      // TODO: if not ?, check that we can't error
      break;

    case TK_SEQ:
      // TODO: type is the type of the last expr
      break;

    case TK_VAR:
    case TK_LET:
      // TODO:
      break;

    case TK_CONTINUE:
      // TODO: check we are in a loop
      break;

    case TK_BREAK:
      // TODO: check we are in a loop
      // TODO: type of loop is unioned with type of expr
      break;

    case TK_RETURN:
      // TODO: type of expr must match type of method
      break;

    case TK_MULTIPLY:
    case TK_DIVIDE:
    case TK_MOD:
    case TK_PLUS:
      // TODO: matching number types
      break;

    case TK_MINUS:
      // TODO: may be unary or binary
      break;

    case TK_LSHIFT:
    case TK_RSHIFT:
      // TODO: check bit length
      break;

    case TK_LT:
    case TK_LE:
    case TK_GE:
    case TK_GT:
      // TODO: ordered
      break;

    case TK_EQ:
    case TK_NE:
      // TODO: comparable
      break;

    case TK_IS:
    case TK_ISNT:
      // TODO:
      break;

    case TK_AND:
    case TK_XOR:
    case TK_OR:
      // TODO:
      break;

    case TK_NOT:
      // TODO:
      break;

    case TK_ASSIGN:
      // TODO:
      break;

    case TK_CONSUME:
      // TODO: path handling
      break;

    case TK_DOT:
      // TODO: type in package, element in tuple, field or method in object,
      // construtor in type
      break;

    case TK_QUALIFY:
      // TODO: find the method
      // TODO: make sure typeargs are within constraints
      break;

    case TK_CALL:
      // TODO: find the method
      // TODO: check arguments match
      // TODO: type is the type of the method call return
      // TODO: mark enclosing as "may error" if we might error
      break;

    case TK_IF:
      // TODO: first must be a Bool
      // TODO: type is union of second and third
      break;

    case TK_WHILE:
      // TODO: first must be a Bool
      // TODO: type is union of second and third, plus any break
      break;

    case TK_REPEAT:
      // TODO: type is the type of the first
      // TODO: second must be a Bool
      break;

    case TK_TRY:
      // TODO: type is the union of first and second
      // TODO: check that the first is marked as "may error"
      break;

    case TK_TUPLE:
      // TODO: create a tuple type for the object
      break;

    case TK_ARRAY:
      // TODO: determine our type by looking at every expr in the array
      break;

    case TK_OBJECT:
      // TODO: create a structural type for the object
      // TODO: make sure it fulfills any traits it claims to have
      break;

    case TK_REFERENCE:
      return typedef_for_reference(ast);

    case TK_THIS:
      return typedef_for_this(ast);

    case TK_INT:
      return typedef_for_builtin(ast, "IntLiteral");

    case TK_FLOAT:
      return typedef_for_builtin(ast, "FloatLiteral");

    case TK_STRING:
      return typedef_for_builtin(ast, "String");

    default: {}
  }

  return true;
}
