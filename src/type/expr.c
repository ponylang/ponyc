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

static ast_t* typedef_for_name(ast_t* ast, const char* name)
{
  size_t line = ast_line(ast);
  size_t pos = ast_line(ast);

  ast_t* type_def = ast_new(TK_TYPEDEF, line, pos, NULL);
  ast_add(type_def, ast_new(TK_NONE, line, pos, NULL));
  ast_add(type_def, ast_new(TK_NONE, line, pos, NULL));

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

static ast_t* expr_bool(ast_t* ast)
{
  ast_t* type = ast_type(ast);
  ast_t* bool_type = typedef_for_name(ast, stringtab("Bool"));

  if(is_subtype(type, bool_type))
    return bool_type;

  ast_free(bool_type);
  return NULL;
}

static ast_t* expr_int(ast_t* ast)
{
  ast_t* type = ast_type(ast);
  ast_t* int_type = typedef_for_name(ast, stringtab("Integer"));
  bool ok = is_subtype(type, int_type);
  ast_free(int_type);

  if(!ok)
    return NULL;

  return ast_dup(type);
}

static ast_t* expr_int_or_bool(ast_t* ast)
{
  ast_t* type = expr_bool(ast);

  if(type == NULL)
    type = expr_int(ast);

  if(type == NULL)
  {
    ast_error(ast, "expected Bool or an integer type");
    return NULL;
  }

  return type;
}

static ast_t* expr_super(ast_t* ast, ast_t* l_type, ast_t* r_type)
{
  if(is_subtype(l_type, r_type))
    return r_type;

  if(is_subtype(r_type, l_type))
    return l_type;

  ast_error(ast, "types are unrelated");
  return NULL;
}

static bool expr_field(ast_t* ast)
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
        "field/param initialiser is not a subtype of the field/param type");
      return false;
    }
  }

  return true;
}

static bool expr_literal(ast_t* ast, const char* name)
{
  ast_t* type_def = typedef_for_name(ast, stringtab(name));

  if(type_def == NULL)
    return false;

  ast_append(ast, type_def);
  return true;
}

static bool expr_this(ast_t* ast)
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
  ast_add(type_def, ast_new(TK_NONE, line, pos, NULL));
  ast_add(type_def, ast_new(TK_NONE, line, pos, NULL));

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
      ast_t* typearg = typedef_for_name(ast, ast_name(typeparam_id));
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

static bool expr_reference(ast_t* ast)
{
  // everything we reference must be in scope
  const char* name = ast_name(ast_child(ast));
  ast_t* def = get_def(ast, name);

  if(def == NULL)
    return false;

  if(!def_before_use(def, ast, name))
    return false;

  switch(ast_id(def))
  {
    case TK_PACKAGE:
      // TODO: only allowed if in a TK_DOT with a type
      break;

    case TK_TYPE:
      // TODO: only allowed if it isn't an alias
      // in which case it is a constructor
      break;

    case TK_CLASS:
    case TK_ACTOR:
      // TODO: could be a constructor, which might be in a TK_CALL,
      // or could be in a TK_DOT with a constructor
      break;

    case TK_FVAR:
    case TK_FLET:
    case TK_PARAM:
    {
      // get the type of the field/parameter and attach it to our reference
      ast_t* type_def = ast_childidx(def, 1);
      assert(ast_id(type_def) == TK_TYPEDEF);
      ast_append(ast, ast_dup(type_def));
      break;
    }

    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      // TODO:
      break;

    case TK_IDSEQ:
      break;

    default:
      assert(0);
  }

  return true;
}

static bool expr_dot(ast_t* ast)
{
  // TODO: type in package, element in tuple, field or method in object,
  // constructor in type
  // ast_t* left = ast_child(ast);
  // ast_t* right = ast_sibling(left);

  return true;
}

static bool expr_identity(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);

  if(expr_super(ast, l_type, r_type) == NULL)
    return false;

  return expr_literal(ast, "Bool");
}

static bool expr_logical(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  ast_t* l_type = expr_int_or_bool(left);
  ast_t* r_type = expr_int_or_bool(right);

  if((l_type == NULL) || (r_type == NULL))
    return false;

  ast_t* type = expr_super(ast, l_type, r_type);

  if(type == NULL)
  {
    ast_free(l_type);
    ast_free(r_type);
    return false;
  }

  if(type == l_type)
    ast_free(r_type);

  if(type == r_type)
    ast_free(l_type);

  ast_append(ast, type);
  return true;
}

static bool expr_not(ast_t* ast)
{
  ast_t* child = ast_child(ast);
  ast_t* type = expr_int_or_bool(child);

  if(type == NULL)
    return false;

  ast_append(ast, type);
  return true;
}

static bool expr_tuple(ast_t* ast)
{
  ast_t* positional = ast_child(ast);
  ast_t* named = ast_sibling(positional);

  if(ast_id(named) != TK_NONE)
  {
    ast_error(named, "named tuple components not yet supported");
    return false;
  }

  size_t line = ast_line(ast);
  size_t pos = ast_line(ast);
  ast_t* seq = ast_child(positional);
  ast_t* type_def;

  if(ast_sibling(seq) == NULL)
  {
    type_def = ast_dup(ast_type(seq));
  } else {
    ast_t* tuple = ast_new(TK_TUPLETYPE, line, pos, NULL);
    ast_add(tuple, ast_dup(ast_type(seq)));
    seq = ast_sibling(seq);
    ast_add(tuple, ast_dup(ast_type(seq)));

    while(seq != NULL)
    {
      ast_t* parent = ast_new(TK_TUPLETYPE, line, pos, NULL);
      ast_add(parent, tuple);
      ast_add(parent, ast_dup(ast_type(seq)));
      tuple = parent;
      seq = ast_sibling(seq);
    }

    type_def = ast_new(TK_TYPEDEF, line, pos, NULL);
    ast_add(type_def, ast_new(TK_NONE, line, pos, NULL));
    ast_add(type_def, ast_new(TK_NONE, line, pos, NULL));
    ast_add(type_def, tuple);
  }

  ast_append(ast, type_def);
  return true;
}

static bool expr_seq(ast_t* ast)
{
  ast_t* last = ast_childlast(ast);
  ast_t* type_def = ast_type(last);
  ast_append(ast, ast_dup(type_def));
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
    case TK_PARAM:
      return expr_field(ast);

    case TK_NEW:
      // TODO: check that the object is fully initialised
      // TODO: if ?, check that we might actually error (but not on a trait)
      // TODO: if not ?, check that we can't error
      // TODO: can only return this
      break;

    case TK_BE:
      // TODO: can only return None
      // TODO: check that we can't error
      break;

    case TK_FUN:
      // TODO: body type must match return types
      // TODO: if ?, check that we might actually error (but not on a trait)
      // TODO: if not ?, check that we can't error
      break;

    case TK_SEQ:
      return expr_seq(ast);

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
      return expr_identity(ast);

    case TK_AND:
    case TK_XOR:
    case TK_OR:
      return expr_logical(ast);

    case TK_NOT:
      return expr_not(ast);

    case TK_ASSIGN:
      // TODO:
      break;

    case TK_CONSUME:
      // TODO: path handling
      break;

    case TK_DOT:
      return expr_dot(ast);

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
      return expr_tuple(ast);

    case TK_ARRAY:
      // TODO: determine our type by looking at every expr in the array
      break;

    case TK_OBJECT:
      // TODO: create a structural type for the object
      // TODO: make sure it fulfills any traits it claims to have
      break;

    case TK_REFERENCE:
      return expr_reference(ast);

    case TK_THIS:
      return expr_this(ast);

    case TK_INT:
      return expr_literal(ast, "IntLiteral");

    case TK_FLOAT:
      return expr_literal(ast, "FloatLiteral");

    case TK_STRING:
      return expr_literal(ast, "String");

    default: {}
  }

  return true;
}
