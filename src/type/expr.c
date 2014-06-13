#include "expr.h"
#include "valid.h"
#include "cap.h"
#include "subtype.h"
#include "typechecker.h"
#include "../ds/stringtab.h"
#include <assert.h>

static ast_t* get_def(ast_t* ast, const char* name)
{
  ast_t* def = ast_get(ast, name);

  if(def == NULL)
  {
    ast_error(ast, "can't find declaration of '%s'", name);
    return NULL;
  }

  return def;
}

static bool def_before_use(ast_t* def, ast_t* use, const char* name)
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
  // TODO: set capability
  ast_t* type_def = ast_from(ast, TK_TYPEDEF);
  ast_add(type_def, ast_from(ast, TK_NONE));
  ast_add(type_def, ast_from(ast, TK_NONE));

  ast_t* nominal = ast_from(ast, TK_NOMINAL);
  ast_add(type_def, nominal);

  ast_add(nominal, ast_from(ast, TK_NONE));
  ast_add(nominal, ast_from_string(ast, name));
  ast_add(nominal, ast_from(ast, TK_NONE));

  if(!valid_nominal(ast, nominal))
  {
    ast_error(nominal, "couldn't create valid type for '%s'", name);
    ast_free(type_def);
    return NULL;
  }

  return type_def;
}

static ast_t* type_bool(ast_t* ast)
{
  ast_t* type = ast_type(ast);
  ast_t* bool_type = typedef_for_name(ast, stringtab("Bool"));

  if(is_subtype(type, bool_type))
    return bool_type;

  ast_free(bool_type);
  return NULL;
}

static ast_t* type_int(ast_t* ast)
{
  ast_t* type = ast_type(ast);
  ast_t* int_type = typedef_for_name(ast, stringtab("Integer"));
  bool ok = is_subtype(type, int_type);
  ast_free(int_type);

  if(!ok)
    return NULL;

  return type;
}

static ast_t* type_int_or_bool(ast_t* ast)
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

static ast_t* type_super(ast_t* l_type, ast_t* r_type)
{
  if((l_type == NULL) || (r_type == NULL))
    return NULL;

  if(is_subtype(l_type, r_type))
    return r_type;

  if(is_subtype(r_type, l_type))
    return l_type;

  return NULL;
}

static ast_t* type_union(ast_t* ast, ast_t* l_type, ast_t* r_type)
{
  ast_t* super = type_super(l_type, r_type);

  if(super != NULL)
    return super;

  ast_t* type = ast_from(ast, TK_UNIONTYPE);
  ast_add(type, r_type);
  ast_add(type, l_type);

  return type;
}

static ast_t* type_for_fun(ast_t* ast)
{
  assert((ast_id(ast) == TK_NEW) ||
    (ast_id(ast) == TK_BE) ||
    (ast_id(ast) == TK_FUN)
    );

  ast_t* cap = ast_child(ast);
  ast_t* id = ast_sibling(cap);
  ast_t* typeparams = ast_sibling(id);
  ast_t* params = ast_sibling(typeparams);
  ast_t* result = ast_sibling(params);
  ast_t* throws = ast_sibling(result);

  ast_t* fun = ast_from(ast, ast_id(ast));
  ast_add(fun, ast_from(ast, TK_NONE));
  ast_add(fun, throws);
  ast_add(fun, result);

  if(ast_id(params) == TK_PARAMS)
  {
    ast_t* types = ast_from(ast, TK_TYPES);
    ast_t* param = ast_child(params);

    while(param != NULL)
    {
      ast_append(types, ast_childidx(param, 1));
      param = ast_sibling(param);
    }

    ast_add(fun, types);
  } else {
    ast_add(fun, params);
  }

  ast_add(fun, typeparams);
  ast_add(fun, id);
  ast_add(fun, cap);

  return fun;
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
    ast_settype(ast, ast_type(init));
    return true;
  }

  if(ast_id(init) != TK_NONE)
  {
    // initializer type must match declared type
    ast_t* init_type = ast_type(init);

    if(!is_subtype(init_type, type))
    {
      ast_error(init,
        "field/param initialiser is not a subtype of the field/param type");
      return false;
    }
  }

  ast_settype(ast, type);
  return true;
}

static bool expr_literal(ast_t* ast, const char* name)
{
  ast_t* type_def = typedef_for_name(ast, stringtab(name));

  if(type_def == NULL)
    return false;

  ast_settype(ast, type_def);
  return true;
}

static bool expr_this(ast_t* ast)
{
  ast_t* def = ast_nearest(ast, TK_TRAIT);

  if(def == NULL)
    def = ast_nearest(ast, TK_CLASS);

  if(def == NULL)
    def = ast_nearest(ast, TK_ACTOR);

  assert(def != NULL);
  ast_t* id = ast_child(def);
  ast_t* typeparams = ast_sibling(id);
  const char* name = ast_name(id);

  ast_t* type_def = ast_from(ast, TK_TYPEDEF);
  ast_add(type_def, ast_from(ast, TK_NONE));
  ast_add(type_def, cap_from_rawcap(ast, cap_for_receiver(ast)));

  ast_t* nominal = ast_from(ast, TK_NOMINAL);
  ast_add(type_def, nominal);

  if(ast_id(typeparams) == TK_TYPEPARAMS)
  {
    ast_t* typeparam = ast_child(typeparams);
    ast_t* typeargs = ast_from(ast, TK_TYPEARGS);
    ast_add(nominal, typeargs);

    while(typeparam != NULL)
    {
      ast_t* typeparam_id = ast_child(typeparam);
      ast_t* typearg = typedef_for_name(ast, ast_name(typeparam_id));
      ast_append(typeargs, typearg);

      typeparam = ast_sibling(typeparam);
    }
  } else {
    ast_add(nominal, ast_from(ast, TK_NONE));
  }

  ast_add(nominal, ast_from_string(ast, name));
  ast_add(nominal, ast_from(ast, TK_NONE));

  if(!valid_nominal(ast, nominal))
  {
    ast_error(nominal, "couldn't create valid type for '%s'", name);
    ast_free(type_def);
    return false;
  }

  ast_settype(ast, type_def);
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
      ast_settype(ast, type_def);
      break;
    }

    case TK_NEW:
    case TK_BE:
    case TK_FUN:
    {
      // method call on 'this'
      ast_settype(ast, type_for_fun(def));
      break;
    }

    case TK_IDSEQ:
      // TODO: local, 'as', or 'for'
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

  ast_error(ast, "not implemented (dot)");
  return false;
}

static bool expr_identity(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);

  if(type_super(l_type, r_type) == NULL)
  {
    ast_error(ast,
      "left and right side must have related types when checking identity");
    return false;
  }

  return expr_literal(ast, "Bool");
}

static bool expr_logical(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  ast_t* l_type = type_int_or_bool(left);
  ast_t* r_type = type_int_or_bool(right);
  ast_t* type = type_super(l_type, r_type);

  if(type == NULL)
  {
    ast_error(ast,
      "left and right side must have related types for logical operations");
  } else {
    ast_settype(ast, type);
  }

  ast_free_unattached(l_type);
  ast_free_unattached(r_type);

  return (type != NULL);
}

static bool expr_not(ast_t* ast)
{
  ast_t* child = ast_child(ast);
  ast_t* type = type_int_or_bool(child);

  if(type == NULL)
    return false;

  ast_settype(ast, type);
  return true;
}

static bool expr_tuple(ast_t* ast)
{
  ast_t* seq = ast_child(ast);
  ast_t* type_def;

  if(ast_sibling(seq) == NULL)
  {
    type_def = ast_type(seq);
  } else {
    ast_t* tuple = ast_from(ast, TK_TUPLETYPE);
    ast_add(tuple, ast_type(seq));
    seq = ast_sibling(seq);
    ast_add(tuple, ast_type(seq));

    while(seq != NULL)
    {
      ast_t* parent = ast_from(ast, TK_TUPLETYPE);
      ast_add(parent, tuple);
      ast_add(parent, ast_type(seq));
      tuple = parent;
      seq = ast_sibling(seq);
    }

    type_def = ast_from(ast, TK_TYPEDEF);
    ast_add(type_def, ast_from(ast, TK_NONE));
    ast_add(type_def, ast_from(ast, TK_NONE));
    ast_add(type_def, tuple);
  }

  ast_settype(ast, type_def);
  return true;
}

static bool expr_call(ast_t* ast)
{
  // TODO: THIS IS NEXT
  // TODO: find the method
  // TODO: check arguments match
  // TODO: type is the type of the method call return
  // TODO: mark enclosing as "may error" if we might error
  ast_t* left = ast_child(ast);
  ast_t* type = ast_type(left);

  switch(ast_id(type))
  {
    case TK_NEW:
    case TK_BE:
    case TK_FUN:
    {
      // method call on 'this'
      // first check if the receiver capability is ok
      token_id rcap = cap_for_receiver(ast);
      token_id fcap = cap_for_fun(type);

      if(!is_cap_sub_cap(rcap, fcap))
      {
        ast_error(ast,
          "receiver capability is not a subtype of method capability");
        return false;
      }

      // TODO: use args to decide unbound type parameters

      // TODO: generate return type for constructors and behaviours
      ast_settype(ast, ast_childidx(type, 4));
      return true;
    }

    default: {}
  }

  assert(0);
  return false;
}

static bool expr_if(ast_t* ast)
{
  ast_t* cond = ast_child(ast);
  ast_t* left = ast_sibling(cond);
  ast_t* right = ast_sibling(left);

  ast_t* bool_type = type_bool(cond);

  if(bool_type == NULL)
  {
    ast_error(cond, "condition must be a Bool");
    return false;
  }

  ast_free(bool_type);

  ast_t* l_type = ast_type(left);
  ast_t* r_type;

  if(ast_id(right) == TK_NONE)
    r_type = typedef_for_name(ast, "None");
  else
    r_type = ast_type(right);

  ast_t* type = type_union(ast, l_type, r_type);
  ast_settype(ast, type);
  return true;
}

static bool expr_seq(ast_t* ast)
{
  ast_t* last = ast_childlast(ast);
  ast_t* type_def = ast_type(last);
  ast_settype(ast, type_def);
  return true;
}

static bool expr_fun(ast_t* ast)
{
  ast_t* impl = ast_childidx(ast, 6);

  if(ast_id(impl) == TK_NONE)
    return true;

  // if specified, body type must match return type
  ast_t* type = ast_childidx(ast, 4);

  if(ast_id(type) != TK_NONE)
  {
    ast_t* body_type = ast_type(impl);

    if(!is_subtype(body_type, type))
    {
      ast_t* last = ast_childlast(impl);
      ast_error(last, "function body type doesn't match function result type");
      ast_error(type, "function result type is here");
      return false;
    }
  }

  // TODO: if ?, check that we might actually error (but not on a trait)
  // TODO: if not ?, check that we can't error
  return true;
}

ast_result_t type_expr(ast_t* ast, int verbose)
{
  switch(ast_id(ast))
  {
    case TK_FVAR:
    case TK_FLET:
    case TK_PARAM:
      if(!expr_field(ast))
        return AST_FATAL;
      break;

    case TK_NEW:
      // TODO: check that the object is fully initialised
      // TODO: if ?, check that we might actually error (but not on a trait)
      // TODO: if not ?, check that we can't error
      // TODO: can only return this
      ast_error(ast, "not implemented (new)");
      return AST_FATAL;

    case TK_BE:
      // TODO: can only return None
      // TODO: check that we can't error
      ast_error(ast, "not implemented (be)");
      return AST_FATAL;

    case TK_FUN:
      if(!expr_fun(ast))
        return AST_FATAL;
      break;

    case TK_SEQ:
      if(!expr_seq(ast))
        return AST_FATAL;
      break;

    case TK_VAR:
    case TK_LET:
      // TODO:
      ast_error(ast, "not implemented (local)");
      return AST_FATAL;

    case TK_CONTINUE:
      // TODO: check we are in a loop
      ast_error(ast, "not implemented (continue)");
      return AST_FATAL;

    case TK_BREAK:
      // TODO: check we are in a loop
      // TODO: type of loop is unioned with type of expr
      ast_error(ast, "not implemented (break)");
      return AST_FATAL;

    case TK_RETURN:
      // TODO: type of expr must match type of method
      ast_error(ast, "not implemented (return)");
      return AST_FATAL;

    case TK_MULTIPLY:
    case TK_DIVIDE:
    case TK_MOD:
    case TK_PLUS:
      // TODO: matching number types
      ast_error(ast, "not implemented (math)");
      return AST_FATAL;

    case TK_MINUS:
      // TODO: may be unary or binary
      ast_error(ast, "not implemented (minus)");
      return AST_FATAL;

    case TK_LSHIFT:
    case TK_RSHIFT:
      // TODO: check bit length
      ast_error(ast, "not implemented (shift)");
      return AST_FATAL;

    case TK_LT:
    case TK_LE:
    case TK_GE:
    case TK_GT:
      // TODO: ordered
      ast_error(ast, "not implemented (order)");
      return AST_FATAL;

    case TK_EQ:
    case TK_NE:
      // TODO: comparable
      ast_error(ast, "not implemented (compare)");
      return AST_FATAL;

    case TK_IS:
    case TK_ISNT:
      if(!expr_identity(ast))
        return AST_FATAL;
      break;

    case TK_AND:
    case TK_XOR:
    case TK_OR:
      if(!expr_logical(ast))
        return AST_FATAL;
      break;

    case TK_NOT:
      if(!expr_not(ast))
        return AST_FATAL;
      break;

    case TK_ASSIGN:
      // TODO:
      ast_error(ast, "not implemented (assign)");
      return AST_FATAL;

    case TK_CONSUME:
      // TODO: path handling
      ast_error(ast, "not implemented (consume)");
      return AST_FATAL;

    case TK_DOT:
      if(!expr_dot(ast))
        return AST_FATAL;
      break;

    case TK_QUALIFY:
      // TODO: find the method
      // TODO: make sure typeargs are within constraints
      ast_error(ast, "not implemented (qualify)");
      return AST_FATAL;

    case TK_CALL:
      if(!expr_call(ast))
        return AST_FATAL;
      break;

    case TK_IF:
      if(!expr_if(ast))
        return AST_FATAL;
      break;

    case TK_WHILE:
      // TODO: first must be a Bool
      // TODO: type is union of second and third, plus any break
      ast_error(ast, "not implemented (while)");
      return AST_FATAL;

    case TK_REPEAT:
      // TODO: type is the type of the first
      // TODO: second must be a Bool
      ast_error(ast, "not implemented (repeat)");
      return AST_FATAL;

    case TK_TRY:
      // TODO: type is the union of first and second
      // TODO: check that the first is marked as "may error"
      ast_error(ast, "not implemented (try)");
      return AST_FATAL;

    case TK_TUPLE:
      return expr_tuple(ast);

    case TK_ARRAY:
      // TODO: determine our type by looking at every expr in the array
      ast_error(ast, "not implemented (array)");
      return AST_FATAL;

    case TK_OBJECT:
      // TODO: create a structural type for the object
      // TODO: make sure it fulfills any traits it claims to have
      ast_error(ast, "not implemented (object)");
      return AST_FATAL;

    case TK_REFERENCE:
      if(!expr_reference(ast))
        return AST_FATAL;
      break;

    case TK_THIS:
      if(!expr_this(ast))
        return AST_FATAL;
      break;

    case TK_INT:
      if(!expr_literal(ast, "IntLiteral"))
        return AST_FATAL;
      break;

    case TK_FLOAT:
      if(!expr_literal(ast, "FloatLiteral"))
        return AST_FATAL;
      break;

    case TK_STRING:
      if(!expr_literal(ast, "String"))
        return AST_FATAL;
      break;

    default: {}
  }

  return AST_OK;
}
