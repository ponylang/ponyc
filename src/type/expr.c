#include "expr.h"
#include "valid.h"
#include "cap.h"
#include "subtype.h"
#include "typechecker.h"
#include "../ds/stringtab.h"
#include <assert.h>

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

static ast_t* typedef_for_tuple(ast_t* ast, int index)
{
  assert(ast_id(ast) == TK_TUPLETYPE);
  ast_t* child = ast_child(ast);

  while(ast_id(child) == TK_TUPLETYPE)
    child = ast_child(child);

  while(index > 0)
  {
    if(child == ast)
      return NULL;

    child = ast_parent(child);
  }

  if(ast_id(ast) == TK_TUPLETYPE)
    ast = ast_childidx(ast, 1);

  assert(ast_id(ast) == TK_TYPEDEF);
  return ast;
}

static ast_t* type_bool(ast_t* ast)
{
  ast_t* type = ast_type(ast);
  ast_t* bool_type = typedef_for_name(ast, NULL, stringtab("Bool"));

  if(is_subtype(ast, type, bool_type))
    return bool_type;

  ast_free(bool_type);
  return NULL;
}

static ast_t* type_int(ast_t* ast)
{
  ast_t* type = ast_type(ast);
  ast_t* int_type = typedef_for_name(ast, NULL, stringtab("Integer"));
  bool ok = is_subtype(ast, type, int_type);
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

static ast_t* type_arithmetic(ast_t* ast)
{
  ast_t* type = ast_type(ast);
  ast_t* a_type = typedef_for_name(ast, NULL, stringtab("Arithmetic"));
  bool ok = is_subtype(ast, type, a_type);
  ast_free(a_type);

  if(!ok)
    return NULL;

  return type;
}

static ast_t* type_super(ast_t* scope, ast_t* l_type, ast_t* r_type)
{
  if((l_type == NULL) || (r_type == NULL))
    return NULL;

  if(is_subtype(scope, l_type, r_type))
    return r_type;

  if(is_subtype(scope, r_type, l_type))
    return l_type;

  return NULL;
}

static ast_t* type_union(ast_t* ast, ast_t* l_type, ast_t* r_type)
{
  ast_t* super = type_super(ast, l_type, r_type);

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

static bool is_lvalue(ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_REFERENCE:
    case TK_DOT:
      // an identifier reference is an lvalue. it may still not be valid to
      // assign to it (it could be a method or an SSA that's already set).
      // the same is true for accessing a member with dot notation.
      return true;

    case TK_TUPLE:
    {
      // a tuple is an lvalue if every component expression is an lvalue
      ast_t* child = ast_child(ast);

      while(child != NULL)
      {
        if(!is_lvalue(child))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    default: {}
  }

  return false;
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

static bool expr_literal(ast_t* ast, const char* name)
{
  ast_t* type_def = typedef_for_name(ast, NULL, stringtab(name));

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
      ast_t* typearg = typedef_for_name(ast, NULL, ast_name(typeparam_id));
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

      // TODO: this tries to validate the type
      ast_t* type = typedef_for_name(ast, NULL, name);
      ast_settype(ast, type);
      return true;
    }

    case TK_FVAR:
    case TK_FLET:
    case TK_PARAM:
    {
      if(!def_before_use(def, ast, name))
        return false;

      // TODO: field/parameter type could be inferred from initialiser
      // get the type of the field/parameter and attach it to our reference
      ast_t* type_def = ast_childidx(def, 1);
      assert(ast_id(type_def) == TK_TYPEDEF);
      ast_settype(ast, type_def);
      return true;
    }

    case TK_NEW:
    case TK_BE:
    case TK_FUN:
    {
      // method call on 'this'
      ast_settype(ast, type_for_fun(def));
      return true;
    }

    case TK_IDSEQ:
    {
      // TODO: local, 'as', or 'for'
      if(!def_before_use(def, ast, name))
        return false;

      return true;
    }

    default: {}
  }

  assert(0);
  return false;
}

static bool expr_dot(ast_t* ast)
{
  // TODO: type in package, element in tuple, field or method in object,
  // constructor in type
  // left is a postfix expression, right is an integer or an id
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* type = ast_type(left);

  switch(ast_id(right))
  {
    case TK_ID:
    {
      if(type == NULL)
      {
        // must be a type in a package
        ast_t* package = ast_get(left, ast_name(left));

        if(package == NULL)
          return false;

        assert(ast_id(package) == TK_PACKAGE);
        const char* package_name = ast_name(left);
        const char* type_name = ast_name(right);
        type = ast_get(package, type_name);

        if(type == NULL)
        {
          ast_error(right, "can't find type '%s' in package '%s'",
            type_name, package_name);
          return false;
        }

        ast_settype(ast, typedef_for_name(ast, package_name, type_name));
        return true;
      }

      ast_error(ast, "not implemented (dot)");
      return false;

      // TODO: field or method access
      return true;
    }

    case TK_INT:
    {
      // element of a tuple
      if((type == NULL) ||
        (ast_id(type) != TK_TYPEDEF) ||
        (ast_id(ast_child(type)) != TK_TUPLETYPE)
        )
      {
        ast_error(right, "member by position can only be used on a tuple");
        return false;
      }

      type = typedef_for_tuple(ast_child(type), ast_int(right));

      if(type == NULL)
      {
        ast_error(right, "tuple index is out of bounds");
        return false;
      }

      // TODO: viewpoint adaptation
      ast_settype(ast, type);
      return true;
    }

    default: {}
  }

  assert(0);
  return false;
}

static bool expr_qualify(ast_t* ast)
{
  // TODO: make sure typeargs are within constraints
  // left is a postfix expression, right is a typeargs
  ast_error(ast, "not implemented (qualify)");
  return false;
}

static bool expr_identity(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);

  if(type_super(ast, l_type, r_type) == NULL)
  {
    ast_error(ast,
      "left and right side must have related types");
    return false;
  }

  return expr_literal(ast, "Bool");
}

static bool expr_arithmetic(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  ast_t* l_type = type_arithmetic(left);
  ast_t* r_type = type_arithmetic(right);
  ast_t* type = type_super(ast, l_type, r_type);

  if(type == NULL)
    ast_error(ast, "left and right side must have related arithmetic types");
  else
    ast_settype(ast, type);

  ast_free_unattached(l_type);
  ast_free_unattached(r_type);

  return (type != NULL);
}

static bool expr_minus(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* l_type = type_arithmetic(left);
  ast_t* r_type = NULL;
  ast_t* type;

  if(right != NULL)
  {
    r_type = type_arithmetic(right);
    type = type_super(ast, l_type, r_type);

    if(type == NULL)
      ast_error(ast, "left and right side must have related arithmetic types");
  } else {
    type = l_type;

    if(type == NULL)
      ast_error(ast, "must have an arithmetic type");
  }

  if(type != NULL)
    ast_settype(ast, type);

  ast_free_unattached(l_type);
  ast_free_unattached(r_type);

  return (type != NULL);
}

static bool expr_shift(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  ast_t* l_type = type_int(left);
  ast_t* r_type = type_int(right);

  if((l_type == NULL) || (r_type == NULL))
  {
    ast_error(ast,
      "left and right side must have integer types");
  } else {
    ast_settype(ast, l_type);
  }

  ast_free_unattached(l_type);
  ast_free_unattached(r_type);

  return (l_type != NULL) && (r_type != NULL);
}

static bool expr_logical(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  ast_t* l_type = type_int_or_bool(left);
  ast_t* r_type = type_int_or_bool(right);
  ast_t* type = type_super(ast, l_type, r_type);

  if(type == NULL)
  {
    ast_error(ast,
      "left and right side must have related integer or boolean types");
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
  ast_t* left = ast_child(ast);
  ast_t* type = ast_type(left);

  switch(ast_id(type))
  {
    case TK_NEW:
    case TK_BE:
    case TK_FUN:
    {
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
      // TODO: mark enclosing as "may error" if we might error
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
    r_type = typedef_for_name(ast, NULL, stringtab("None"));
  else
    r_type = ast_type(right);

  ast_t* type = type_union(ast, l_type, r_type);
  ast_settype(ast, type);
  return true;
}

static bool expr_while(ast_t* ast)
{
  // TODO: break statements
  return expr_if(ast);
}

static bool expr_repeat(ast_t* ast)
{
  ast_t* cond = ast_child(ast);
  ast_t* body = ast_sibling(cond);

  ast_t* bool_type = type_bool(cond);

  if(bool_type == NULL)
  {
    ast_error(cond, "condition must be a Bool");
    return false;
  }

  // TODO: break statements
  ast_free(bool_type);
  ast_settype(ast, ast_type(body));
  return true;
}

static bool expr_assign(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);

  if(!is_lvalue(left))
  {
    ast_error(ast, "left side must be something that can be assigned to");
    return false;
  }

  if(!is_subtype(ast, r_type, l_type))
  {
    ast_error(ast, "right side must be a subtype of left side");
    return false;
  }

  // TODO: viewpoint adaptation
  // TODO: disallow reassignment to SSA variable
  ast_settype(ast, l_type);
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

    if(!is_subtype(ast, body_type, type))
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
      if(!expr_arithmetic(ast))
        return AST_FATAL;
      break;

    case TK_MINUS:
      if(!expr_minus(ast))
        return AST_FATAL;
      break;

    case TK_LSHIFT:
    case TK_RSHIFT:
      if(!expr_shift(ast))
        return AST_FATAL;
      break;

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
      if(!expr_assign(ast))
        return AST_FATAL;
      break;

    case TK_CONSUME:
      // TODO: path handling
      ast_error(ast, "not implemented (consume)");
      return AST_FATAL;

    case TK_DOT:
      if(!expr_dot(ast))
        return AST_FATAL;
      break;

    case TK_QUALIFY:
      if(!expr_qualify(ast))
        return AST_FATAL;
      break;

    case TK_CALL:
      if(!expr_call(ast))
        return AST_FATAL;
      break;

    case TK_IF:
      if(!expr_if(ast))
        return AST_FATAL;
      break;

    case TK_WHILE:
      if(!expr_while(ast))
        return AST_FATAL;
      break;

    case TK_REPEAT:
      if(!expr_repeat(ast))
        return AST_FATAL;
      break;

    case TK_TRY:
      // TODO: type is the union of first and second
      // TODO: check that the first is marked as "may error"
      ast_error(ast, "not implemented (try)");
      return AST_FATAL;

    case TK_TUPLE:
      if(!expr_tuple(ast))
        return AST_FATAL;
      break;

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

ast_t* typedef_for_name(ast_t* ast, const char* package, const char* name)
{
  // TODO: capability
  ast_t* type_def = ast_from(ast, TK_TYPEDEF);
  ast_add(type_def, ast_from(ast, TK_NONE));
  ast_add(type_def, ast_from(ast, TK_NONE));

  ast_t* nominal = ast_from(ast, TK_NOMINAL);
  ast_add(type_def, nominal);

  ast_add(nominal, ast_from(ast, TK_NONE));
  ast_add(nominal, ast_from_string(ast, name));
  ast_add(nominal, ast_from_string(ast, package));

  if(!valid_nominal(ast, nominal))
  {
    ast_error(nominal, "couldn't create valid type for '%s'", name);
    ast_free(type_def);
    return NULL;
  }

  return type_def;
}
