#include "postfix.h"
#include "reference.h"
#include "literal.h"
#include "control.h"
#include "../pkg/package.h"
#include "../pass/expr.h"
#include "../type/reify.h"
#include "../type/subtype.h"
#include "../type/assemble.h"
#include "../type/lookup.h"
#include "../type/alias.h"
#include "../type/cap.h"
#include "../ast/token.h"
#include <assert.h>

static bool expr_packageaccess(pass_opt_t* opt, ast_t* ast)
{
  // left is a packageref, right is an id
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* type = ast_type(left);

  assert(ast_id(left) == TK_PACKAGEREF);
  assert(ast_id(right) == TK_ID);

  // must be a type in a package
  const char* package_name = ast_name(ast_child(left));
  ast_t* package = ast_get(left, package_name, NULL);

  if(package == NULL)
  {
    ast_error(right, "can't access package '%s'", package_name);
    return false;
  }

  assert(ast_id(package) == TK_PACKAGE);
  const char* type_name = ast_name(right);
  type = ast_get(package, type_name, NULL);

  if(type == NULL)
  {
    ast_error(right, "can't find type '%s' in package '%s'",
      type_name, package_name);
    return false;
  }

  ast_settype(ast, type_sugar(ast, package_name, type_name));
  ast_setid(ast, TK_TYPEREF);
  return expr_typeref(opt, ast);
}

static bool expr_typeaccess(pass_opt_t* opt, ast_t* ast)
{
  typecheck_t* t = &opt->check;

  // left is a typeref, right is an id
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* type = ast_type(left);

  assert(ast_id(left) == TK_TYPEREF);
  assert(ast_id(right) == TK_ID);

  ast_t* find = lookup(t, ast, type, ast_name(right));

  if(find == NULL)
    return false;

  bool ret = true;

  switch(ast_id(find))
  {
    case TK_TYPEPARAM:
    {
      ast_error(right, "can't look up a typeparam on a type");
      ret = false;
      break;
    }

    case TK_NEW:
    {
      ast_t* def = (ast_t*)ast_data(type);

      if((ast_id(type) == TK_NOMINAL) && (ast_id(def) == TK_ACTOR))
        ast_setid(ast, TK_NEWBEREF);
      else
        ast_setid(ast, TK_NEWREF);

      ast_settype(ast, type_for_fun(find));

      if(ast_id(ast_childidx(find, 5)) == TK_QUESTION)
        ast_seterror(ast);
      break;
    }

    case TK_FVAR:
    case TK_FLET:
    case TK_BE:
    case TK_FUN:
    {
      // Make this a lookup on a default constructed object.
      ast_free_unattached(find);

      ast_t* dot = ast_from(ast, TK_DOT);
      ast_add(dot, ast_from_string(ast, "create"));
      ast_swap(left, dot);
      ast_add(dot, left);

      if(!expr_dot(opt, dot))
        return false;

      ast_t* call = ast_from(ast, TK_CALL);
      ast_swap(dot, call);
      ast_add(call, dot); // the LHS goes at the end, not the beginning
      ast_add(call, ast_from(ast, TK_NONE)); // named
      ast_add(call, ast_from(ast, TK_NONE)); // positional

      if(!expr_call(opt, call))
        return false;

      return expr_dot(opt, ast);
    }

    default:
    {
      assert(0);
      ret = false;
      break;
    }
  }

  ast_free_unattached(find);
  return ret;
}

static bool make_tuple_index(ast_t** astp)
{
  ast_t* ast = *astp;
  const char* name = ast_name(ast);

  if(name[0] != '_')
    return false;

  for(size_t i = 1; name[i] != '\0'; i++)
  {
    if((name[i] < '0') || (name[i] > '9'))
      return false;
  }

  size_t index = strtol(&name[1], NULL, 10) - 1;
  ast_t* node = ast_from_int(ast, index);
  ast_replace(astp, node);

  return true;
}

static bool expr_tupleaccess(ast_t* ast)
{
  // Left is a postfix expression, right is a lookup name.
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* type = ast_type(left);

  // Change the lookup name to an integer index.
  if(!make_tuple_index(&right))
  {
    ast_error(right,
      "lookup on a tuple must take the form _X, where X is an integer");
    return false;
  }

  // Make sure our index is in bounds.
  type = ast_childidx(type, (size_t)ast_int(right));

  if(type == NULL)
  {
    ast_error(right, "tuple index is out of bounds");
    return false;
  }

  ast_setid(ast, TK_FLETREF);
  ast_settype(ast, type);
  ast_inheriterror(ast);
  return true;
}

static bool expr_memberaccess(typecheck_t* t, ast_t* ast)
{
  // Left is a postfix expression, right is an id.
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* type = ast_type(left);

  assert(ast_id(right) == TK_ID);

  ast_t* find = lookup(t, ast, type, ast_name(right));

  if(find == NULL)
    return false;

  bool ret = true;

  switch(ast_id(find))
  {
    case TK_TYPEPARAM:
    {
      ast_error(right, "can't look up a typeparam on an expression");
      ret = false;
      break;
    }

    case TK_FVAR:
    {
      if(!expr_fieldref(ast, left, find, TK_FVARREF))
        return false;
      break;
    }

    case TK_FLET:
    {
      if(!expr_fieldref(ast, left, find, TK_FLETREF))
        return false;
      break;
    }

    case TK_NEW:
    {
      ast_error(right, "can't look up a constructor on an expression");
      ret = false;
      break;
    }

    case TK_BE:
    case TK_FUN:
    {
      if(ast_id(find) == TK_BE)
        ast_setid(ast, TK_BEREF);
      else
        ast_setid(ast, TK_FUNREF);

      ast_settype(ast, type_for_fun(find));

      if(ast_id(ast_childidx(find, 5)) == TK_QUESTION)
        ast_seterror(ast);
      break;
    }

    default:
    {
      assert(0);
      ret = false;
      break;
    }
  }

  ast_free_unattached(find);
  ast_inheriterror(ast);
  return ret;
}

bool expr_qualify(pass_opt_t* opt, ast_t* ast)
{
  // left is a postfix expression, right is a typeargs
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* type = ast_type(left);
  assert(ast_id(right) == TK_TYPEARGS);

  switch(ast_id(left))
  {
    case TK_TYPEREF:
    {
      // qualify the type
      assert(ast_id(type) == TK_NOMINAL);

      if(ast_id(ast_childidx(type, 2)) != TK_NONE)
      {
        ast_error(ast, "can't qualify an already qualified type");
        return false;
      }

      type = ast_dup(type);
      ast_t* typeargs = ast_childidx(type, 2);
      ast_replace(&typeargs, right);
      ast_settype(ast, type);
      ast_setid(ast, TK_TYPEREF);

      return expr_typeref(opt, ast);
    }

    case TK_NEWREF:
    case TK_NEWBEREF:
    case TK_BEREF:
    case TK_FUNREF:
    case TK_NEWAPP:
    case TK_NEWBEAPP:
    case TK_BEAPP:
    case TK_FUNAPP:
    {
      // qualify the function
      assert(ast_id(type) == TK_FUNTYPE);
      ast_t* typeparams = ast_childidx(type, 1);

      if(!check_constraints(typeparams, right, true))
        return false;

      type = reify(type, typeparams, right);
      typeparams = ast_childidx(type, 1);
      ast_replace(&typeparams, ast_from(typeparams, TK_NONE));

      ast_settype(ast, type);
      ast_setid(ast, ast_id(left));
      ast_inheriterror(ast);
      return true;
    }

    default: {}
  }

  assert(0);
  return false;
}

bool expr_dot(pass_opt_t* opt, ast_t* ast)
{
  typecheck_t* t = &opt->check;

  // Left is a postfix expression, right is an id.
  ast_t* left = ast_child(ast);

  switch(ast_id(left))
  {
    case TK_PACKAGEREF:
      return expr_packageaccess(opt, ast);

    case TK_TYPEREF:
      return expr_typeaccess(opt, ast);

    default: {}
  }

  ast_t* type = ast_type(left);

  if(type == NULL)
  {
    ast_error(ast, "invalid left hand side");
    return false;
  }

  if(is_type_literal(type))
    return coerce_literal_member(ast);

  if(ast_id(type) == TK_TUPLETYPE)
    return expr_tupleaccess(ast);

  return expr_memberaccess(t, ast);
}

bool expr_tilde(pass_opt_t* opt, ast_t* ast)
{
  if(!expr_dot(opt, ast))
    return false;

  switch(ast_id(ast))
  {
    case TK_NEWREF:
      ast_setid(ast, TK_NEWAPP);
      return true;

    case TK_NEWBEREF:
      ast_setid(ast, TK_NEWBEAPP);
      return true;

    case TK_BEREF:
      ast_setid(ast, TK_BEAPP);
      return true;

    case TK_FUNREF:
      ast_setid(ast, TK_FUNAPP);
      return true;

    case TK_TYPEREF:
      ast_error(ast, "can't do partial application on a package");
      return false;

    case TK_FVARREF:
    case TK_FLETREF:
      ast_error(ast, "can't do partial application of a field");
      return false;

    default: {}
  }

  assert(0);
  return false;
}

static bool expr_apply(pass_opt_t* opt, ast_t* ast)
{
  // Sugar .apply()
  AST_GET_CHILDREN(ast, positional, namedargs, lhs);

  ast_t* dot = ast_from(ast, TK_DOT);
  ast_add(dot, ast_from_string(ast, "apply"));
  ast_swap(lhs, dot);
  ast_add(dot, lhs);

  if(!expr_dot(opt, dot))
    return false;

  return expr_call(opt, ast);
}

static bool is_this_incomplete(typecheck_t* t, ast_t* ast)
{
  // If we're in a field initialiser, we're incomplete by definition.
  if(t->frame->method == NULL)
    return true;

  // If we're not in a constructor, we're complete by definition.
  if(ast_id(t->frame->method) != TK_NEW)
    return false;

  // Check if all fields have been marked as defined.
  ast_t* members = ast_childidx(t->frame->type, 4);
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FLET:
      case TK_FVAR:
      {
        sym_status_t status;
        ast_t* id = ast_child(member);
        ast_get(ast, ast_name(id), &status);

        if(status != SYM_DEFINED)
          return true;

        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  return false;
}

static bool check_type_params(ast_t* lhs)
{
  ast_t* type = ast_type(lhs);
  ast_t* typeparams = ast_childidx(type, 1);

  if(ast_id(typeparams) != TK_NONE)
  {
    ast_error(lhs, "can't call a function with unqualified type parameters");
    return false;
  }

  return true;
}

static bool extend_positional_args(ast_t* params, ast_t* positional)
{
  // Fill out the positional args to be as long as the param list.
  size_t param_len = ast_childcount(params);
  size_t arg_len = ast_childcount(positional);

  if(arg_len > param_len)
  {
    ast_error(positional, "too many arguments");
    return false;
  }

  while(arg_len < param_len)
  {
    ast_append(positional, ast_from(positional, TK_NONE));
    arg_len++;
  }

  return true;
}

static bool apply_named_args(ast_t* params, ast_t* positional, ast_t* namedargs)
{
  ast_t* namedarg = ast_child(namedargs);

  while(namedarg != NULL)
  {
    AST_GET_CHILDREN(namedarg, arg_id, arg);

    ast_t* param = ast_child(params);
    size_t param_index = 0;

    while(param != NULL)
    {
      AST_GET_CHILDREN(param, param_id);

      if(ast_name(arg_id) == ast_name(param_id))
        break;

      param = ast_sibling(param);
      param_index++;
    }

    if(param == NULL)
    {
      ast_error(arg_id, "not a parameter name");
      return false;
    }

    ast_t* arg_replace = ast_childidx(positional, param_index);

    if(ast_id(arg_replace) != TK_NONE)
    {
      ast_error(arg_id, "named argument is already supplied");
      ast_error(arg_replace, "supplied argument is here");
      return false;
    }

    ast_replace(&arg_replace, arg);
    namedarg = ast_sibling(namedarg);
  }

  return true;
}

static bool apply_default_arg(pass_opt_t* opt, ast_t* param, ast_t* arg)
{
  // Pick up a default argument.
  ast_t* def_arg = ast_childidx(param, 2);

  if(ast_id(def_arg) == TK_NONE)
  {
    ast_error(arg, "not enough arguments");
    return false;
  }

  ast_setid(arg, TK_SEQ);
  ast_add(arg, def_arg);

  // Type check the arg.
  if(ast_type(def_arg) == NULL)
  {
    if(ast_visit(&arg, NULL, pass_expr, opt) != AST_OK)
      return false;
  } else {
    if(!expr_seq(arg))
      return false;
  }

  return true;
}

static bool check_arg_types(pass_opt_t* opt, ast_t* params, ast_t* positional,
  bool incomplete, bool partial)
{
  // Check positional args vs params.
  ast_t* param = ast_child(params);
  ast_t* arg = ast_child(positional);

  while(arg != NULL)
  {
    if(ast_id(arg) == TK_NONE)
    {
      if(partial)
      {
        // Don't check missing arguments for partial application.
        arg = ast_sibling(arg);
        param = ast_sibling(param);
        continue;
      } else {
        // Pick up a default argument if we can.
        if(!apply_default_arg(opt, param, arg))
          return false;
      }
    }

    ast_t* p_type = ast_childidx(param, 1);

    if(!coerce_literals(arg, p_type))
      return false;

    ast_t* arg_type = ast_type(arg);

    if(arg_type == NULL)
    {
      ast_error(arg, "can't use return, break or continue in an argument");
      return false;
    } else if(ast_id(arg_type) == TK_FUNTYPE) {
      ast_error(arg, "can't use a method as an argument");
      return false;
    }

    ast_t* a_type = alias(arg_type);

    if(incomplete)
    {
      ast_t* expr = ast_child(arg);

      // If 'this' is incomplete and the arg is 'this', change the type to tag.
      if((ast_id(expr) == TK_THIS) && (ast_sibling(expr) == NULL))
      {
        ast_t* tag_type = set_cap_and_ephemeral(a_type, TK_TAG, TK_NONE);
        ast_free_unattached(a_type);
        a_type = tag_type;
      }
    }

    if(!is_subtype(a_type, p_type))
    {
      ast_error(arg, "argument not a subtype of parameter");
      ast_error(p_type, "parameter type: %s", ast_print_type(p_type));
      ast_error(a_type, "argument type: %s", ast_print_type(a_type));

      ast_free_unattached(a_type);
      return false;
    }

    ast_free_unattached(a_type);
    arg = ast_sibling(arg);
    param = ast_sibling(param);
  }

  return true;
}

static bool auto_recover_call(ast_t* ast, ast_t* positional, ast_t* result)
{
  // We can recover the receiver (ie not alias the receiver type) if all
  // arguments are sendable and the result is either sendable or unused.
  if(is_result_needed(ast) && !sendable(result))
    return false;

  ast_t* arg = ast_child(positional);

  while(arg != NULL)
  {
    if(ast_id(arg) != TK_NONE)
    {
      ast_t* arg_type = ast_type(arg);
      ast_t* a_type = alias(arg_type);

      bool ok = sendable(a_type);
      ast_free_unattached(a_type);

      if(!ok)
        return false;
    }

    arg = ast_sibling(arg);
  }

  return true;
}

static bool check_receiver_cap(ast_t* ast, bool incomplete)
{
  AST_GET_CHILDREN(ast, positional, namedargs, lhs);

  ast_t* type = ast_type(lhs);
  AST_GET_CHILDREN(type, cap, typeparams, params, result);

  // Check receiver cap.
  ast_t* receiver = ast_child(lhs);

  // Dig through function qualification.
  if(ast_id(receiver) == TK_FUNREF)
    receiver = ast_child(receiver);

  // Receiver type, alias of receiver type, and target type.
  ast_t* r_type = ast_type(receiver);
  ast_t* t_type = set_cap_and_ephemeral(r_type, ast_id(cap), TK_NONE);
  ast_t* a_type;

  // If we can recover the receiver, we don't alias it here.
  bool can_recover = auto_recover_call(ast, positional, result);

  if(can_recover)
    a_type = r_type;
  else
    a_type = alias(r_type);

  if(incomplete && (ast_id(receiver) == TK_THIS))
  {
    // If 'this' is incomplete and the arg is 'this', change the type to tag.
    ast_t* tag_type = set_cap_and_ephemeral(a_type, TK_TAG, TK_NONE);

    if(a_type != r_type)
      ast_free_unattached(a_type);

    a_type = tag_type;
  } else {
    incomplete = false;
  }

  bool ok = is_subtype(a_type, t_type);

  if(!ok)
  {
    ast_error(ast,
      "receiver capability is not a subtype of method capability");
    ast_error(receiver, "receiver type: %s", ast_print_type(a_type));
    ast_error(cap, "target type: %s", ast_print_type(t_type));

    if(!can_recover && is_subtype(r_type, t_type))
    {
      ast_error(ast,
        "this would be possible if the arguments and return value "
        "were all sendable");
    }

    if(incomplete && is_subtype(r_type, t_type))
    {
      ast_error(ast,
        "this would be possible if all the field of 'this' were assigned to "
        "at this point");
    }
  }

  if(a_type != r_type)
    ast_free_unattached(a_type);

  ast_free_unattached(r_type);
  ast_free_unattached(t_type);
  return ok;
}

static bool expr_methodapplication(pass_opt_t* opt, ast_t* ast, bool partial)
{
  // TODO: use args to decide unbound type parameters
  AST_GET_CHILDREN(ast, positional, namedargs, lhs);

  ast_t* type = ast_type(lhs);
  AST_GET_CHILDREN(type, cap, typeparams, params, result);

  if(!check_type_params(lhs))
    return false;

  if(!extend_positional_args(params, positional))
    return false;

  if(!apply_named_args(params, positional, namedargs))
    return false;

  bool incomplete = is_this_incomplete(&opt->check, ast);

  if(!check_arg_types(opt, params, positional, incomplete, partial))
    return false;

  switch(ast_id(lhs))
  {
    case TK_FUNREF:
    case TK_FUNAPP:
      if(!check_receiver_cap(ast, incomplete))
        return false;
      break;

    default: {}
  }

  return true;
}

static bool expr_methodcall(pass_opt_t* opt, ast_t* ast)
{
  if(!expr_methodapplication(opt, ast, false))
    return false;

  AST_GET_CHILDREN(ast, positional, namedargs, lhs);
  ast_t* type = ast_type(lhs);
  AST_GET_CHILDREN(type, cap, typeparams, params, result);

  ast_settype(ast, result);
  ast_inheriterror(ast);
  return true;
}

static bool expr_partialapplication(pass_opt_t* opt, ast_t* ast)
{
  typecheck_t* t = &opt->check;

  if(!expr_methodapplication(opt, ast, true))
    return false;

  AST_GET_CHILDREN(ast, positional, namedargs, lhs);
  ast_t* type = ast_type(lhs);
  AST_GET_CHILDREN(type, cap, typeparams, params, result);

  // Create a new anonymous type.
  ast_t* c_id = ast_from_string(ast, package_hygienic_id(t));

  BUILD(def, ast,
    NODE(TK_CLASS, AST_SCOPE
      TREE(c_id)
      NONE
      NONE
      NONE
      NODE(TK_MEMBERS)
      NONE
      NONE));

  // We will have a create method in the type.
  BUILD(create, ast,
    NODE(TK_NEW, AST_SCOPE
      NONE
      ID("create")
      NONE
      NODE(TK_PARAMS)
      NONE
      NONE
      NODE(TK_SEQ)
      NODE(TK_DBLARROW)));

  // We will have an apply method in the type.
  token_id can_error = ast_canerror(lhs) ? TK_QUESTION : TK_NONE;

  BUILD(apply, ast,
    NODE(TK_FUN, AST_SCOPE
      TREE(cap)
      ID("apply")
      NONE
      NODE(TK_PARAMS)
      TREE(result)
      NODE(can_error)
      NODE(TK_SEQ)
      NODE(TK_DBLARROW)));

  // We will replace partial application with $0.create(...)
  BUILD(call, ast,
    NODE(TK_CALL,
      NONE
      NODE(TK_NAMEDARGS)
      NODE(TK_DOT, NODE(TK_REFERENCE, TREE(c_id)) ID("create"))));

  ast_t* create_params = ast_childidx(create, 3);
  ast_t* create_body = ast_childidx(create, 6);
  ast_t* apply_params = ast_childidx(apply, 3);
  // ast_t* apply_body = ast_childidx(apply, 6);
  ast_t* call_namedargs = ast_childidx(call, 1);
  ast_t* class_members = ast_childidx(def, 4);

  ast_t* arg = ast_child(positional);
  ast_t* param = ast_child(params);

  while(arg != NULL)
  {
    if(ast_id(arg) == TK_NONE)
    {
      // A parameter of the apply method, using the same name, type and default
      // argument.
      ast_append(apply_params, param);
    } else {
      AST_GET_CHILDREN(param, id, type);
      ast_t* p_id = ast_from_string(id, package_hygienic_id(t));

      // A field in the type.
      BUILD(field, arg, NODE(TK_FLET, TREE(id) TREE(type) NONE));

      // A parameter of the constructor.
      BUILD(ctor_param, arg, NODE(TK_PARAM, TREE(p_id) TREE(type) NONE));

      // An assignment in the constructor body.
      BUILD(assign, arg,
        NODE(TK_ASSIGN,
          NODE(TK_REFERENCE, TREE(id))
          NODE(TK_CONSUME, TREE(p_id))));

      // A named argument at the call site.
      BUILD(call_arg, arg, NODE(TK_NAMEDARG, TREE(p_id) TREE(arg)));

      // TODO: The receiver is this as well.
      ast_append(class_members, field);
      ast_append(create_params, ctor_param);
      ast_append(create_body, assign);
      ast_append(call_namedargs, call_arg);
    }

    arg = ast_sibling(arg);
    param = ast_sibling(param);
  }

  // TODO:
  // apply body
  // replace the expression with a create call
  // typecheck the anonymous type

  ast_error(ast, "not implemented: partial application");
  return false;
}

bool expr_call(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, positional, namedargs, lhs);
  ast_t* type = ast_type(lhs);

  if(!coerce_literal_operator(ast))
    return false;

  // Type already set by literal handler
  if(ast_type(ast) != NULL)
    return true;

  if(type != NULL && is_type_literal(type))
  {
    ast_error(ast, "Cannot call a literal");
    return false;
  }

  switch(ast_id(lhs))
  {
    case TK_STRING:
    case TK_ARRAY:
    case TK_OBJECT:
    case TK_TUPLE:
    case TK_THIS:
    case TK_FVARREF:
    case TK_FLETREF:
    case TK_VARREF:
    case TK_LETREF:
    case TK_PARAMREF:
    case TK_CALL:
      return expr_apply(opt, ast);

    case TK_NEWREF:
    case TK_NEWBEREF:
    case TK_BEREF:
    case TK_FUNREF:
      return expr_methodcall(opt, ast);

    case TK_NEWAPP:
    case TK_NEWBEAPP:
    case TK_BEAPP:
    case TK_FUNAPP:
      return expr_partialapplication(opt, ast);

    default: {}
  }

  assert(0);
  return false;
}
