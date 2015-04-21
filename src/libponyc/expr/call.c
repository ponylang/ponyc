#include "call.h"
#include "postfix.h"
#include "control.h"
#include "literal.h"
#include "reference.h"
#include "../pkg/package.h"
#include "../pass/expr.h"
#include "../type/alias.h"
#include "../type/assemble.h"
#include "../type/reify.h"
#include "../type/subtype.h"
#include "../type/viewpoint.h"
#include <assert.h>

static bool insert_apply(pass_opt_t* opt, ast_t** astp)
{
  // Sugar .apply()
  ast_t* ast = *astp;
  AST_GET_CHILDREN(ast, positional, namedargs, lhs);

  ast_t* dot = ast_from(ast, TK_DOT);
  ast_add(dot, ast_from_string(ast, "apply"));
  ast_swap(lhs, dot);
  ast_add(dot, lhs);

  if(!expr_dot(opt, &dot))
    return false;

  return expr_call(opt, astp);
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

static bool check_type_params(ast_t** astp)
{
  ast_t* lhs = *astp;
  ast_t* type = ast_type(lhs);

  if(is_typecheck_error(type))
    return false;

  ast_t* typeparams = ast_childidx(type, 1);
  assert(ast_id(type) == TK_FUNTYPE);

  if(ast_id(typeparams) == TK_NONE)
    return true;

  BUILD(typeargs, typeparams, NODE(TK_TYPEARGS));

  if(!check_constraints(typeparams, typeargs, true))
  {
    ast_free_unattached(typeargs);
    return false;
  }

  type = reify(type, typeparams, typeargs);
  typeparams = ast_childidx(type, 1);
  ast_replace(&typeparams, ast_from(typeparams, TK_NONE));

  REPLACE(astp, NODE(ast_id(lhs), TREE(lhs) TREE(typeargs)));
  ast_settype(*astp, type);

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
    ast_setid(positional, TK_POSITIONALARGS);
    ast_append(positional, ast_from(positional, TK_NONE));
    arg_len++;
  }

  return true;
}

static bool apply_named_args(ast_t* params, ast_t* positional,
  ast_t* namedargs)
{
  ast_t* namedarg = ast_pop(namedargs);

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
      if(ast_id(namedarg) == TK_UPDATEARG)
      {
        ast_error(arg_id,
          "cannot use sugar, update() has no parameter named \"value\"");
        return false;
      }

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

    // Extract named argument expression to avoid copying it
    ast_free(ast_pop(namedarg));  // ID
    arg = ast_pop(namedarg);  // Expression

    ast_replace(&arg_replace, arg);
    namedarg = ast_pop(namedargs);
  }

  ast_setid(namedargs, TK_NONE);
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

    ast_visit(&arg, NULL, pass_nodebug, opt);
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

    if(!coerce_literals(&arg, p_type, opt))
      return false;

    ast_t* arg_type = ast_type(arg);

    if(is_control_type(arg_type))
    {
      ast_error(arg, "can't use a control expression in an argument");
      return false;
    }

    if(is_typecheck_error(arg_type))
      return false;

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
      ast_error(param, "parameter type: %s", ast_print_type(p_type));
      ast_error(arg, "argument type: %s", ast_print_type(a_type));

      ast_free_unattached(a_type);
      return false;
    }

    ast_free_unattached(a_type);
    arg = ast_sibling(arg);
    param = ast_sibling(param);
  }

  return true;
}

static bool auto_recover_call(ast_t* ast, ast_t* receiver_type,
  ast_t* positional, ast_t* result)
{
  // We can recover the receiver (ie not alias the receiver type) if all
  // arguments are safe and the result is either safe or unused.
  if(is_result_needed(ast) && !safe_to_autorecover(receiver_type, result))
    return false;

  ast_t* arg = ast_child(positional);

  while(arg != NULL)
  {
    if(ast_id(arg) != TK_NONE)
    {
      ast_t* arg_type = ast_type(arg);

      if(is_typecheck_error(arg_type))
        return false;

      ast_t* a_type = alias(arg_type);
      bool ok = safe_to_autorecover(receiver_type, a_type);
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

  if(is_typecheck_error(type))
    return false;

  AST_GET_CHILDREN(type, cap, typeparams, params, result);

  // Check receiver cap.
  ast_t* receiver = ast_child(lhs);

  // Dig through function qualification.
  if(ast_id(receiver) == TK_FUNREF)
    receiver = ast_child(receiver);

  // Receiver type, alias of receiver type, and target type.
  ast_t* r_type = ast_type(receiver);

  if(is_typecheck_error(r_type))
    return false;

  ast_t* t_type = set_cap_and_ephemeral(r_type, ast_id(cap), TK_NONE);
  ast_t* a_type;

  // If we can recover the receiver, we don't alias it here.
  bool can_recover = auto_recover_call(ast, r_type, positional, result);
  bool cap_recover = false;

  switch(ast_id(cap))
  {
    case TK_ISO:
    case TK_TRN:
    case TK_VAL:
    case TK_TAG:
      break;

    case TK_REF:
    case TK_BOX:
      cap_recover = true;
      break;

    default:
      assert(0);
  }

  if(can_recover && cap_recover)
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

    if(!can_recover && cap_recover && is_subtype(r_type, t_type))
    {
      ast_error(ast,
        "this would be possible if the arguments and return value "
        "were all sendable");
    }

    if(incomplete && is_subtype(r_type, t_type))
    {
      ast_error(ast,
        "this would be possible if all the fields of 'this' were assigned to "
        "at this point");
    }
  }

  if(a_type != r_type)
    ast_free_unattached(a_type);

  ast_free_unattached(r_type);
  ast_free_unattached(t_type);
  return ok;
}

static bool method_application(pass_opt_t* opt, ast_t* ast, bool partial)
{
  // TODO: use args to decide unbound type parameters
  AST_GET_CHILDREN(ast, positional, namedargs, lhs);

  if(!check_type_params(&lhs))
    return false;

  ast_t* type = ast_type(lhs);

  if(is_typecheck_error(type))
    return false;

  AST_GET_CHILDREN(type, cap, typeparams, params, result);

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

static bool method_call(pass_opt_t* opt, ast_t* ast)
{
  if(!method_application(opt, ast, false))
    return false;

  AST_GET_CHILDREN(ast, positional, namedargs, lhs);
  ast_t* type = ast_type(lhs);

  if(is_typecheck_error(type))
    return false;

  AST_GET_CHILDREN(type, cap, typeparams, params, result);
  ast_settype(ast, result);
  ast_inheriterror(ast);

  return true;
}

static token_id partial_application_cap(ast_t* ftype, ast_t* receiver,
  ast_t* positional)
{
  // Check if the apply method in the generated object literal can accept a box
  // receiver. If not, it must be a ref receiver. It can accept a box receiver
  // if box->receiver <: lhs->receiver and box->arg <: lhs->param.
  AST_GET_CHILDREN(ftype, cap, typeparams, params, result);

  ast_t* type = ast_type(receiver);
  ast_t* view_type = viewpoint_cap(TK_BOX, TK_NONE, type);
  ast_t* need_type = set_cap_and_ephemeral(type, ast_id(cap), TK_NONE);

  bool ok = is_subtype(view_type, need_type);
  ast_free_unattached(view_type);
  ast_free_unattached(need_type);

  if(!ok)
    return TK_REF;

  ast_t* param = ast_child(params);
  ast_t* arg = ast_child(positional);

  while(arg != NULL)
  {
    if(ast_id(arg) != TK_NONE)
    {
      type = ast_type(arg);
      view_type = viewpoint_cap(TK_BOX, TK_NONE, type);
      need_type = ast_childidx(param, 1);

      ok = is_subtype(view_type, need_type);
      ast_free_unattached(view_type);
      ast_free_unattached(need_type);

      if(!ok)
        return TK_REF;
    }

    arg = ast_sibling(arg);
    param = ast_sibling(param);
  }

  return TK_BOX;
}

static bool partial_application(pass_opt_t* opt, ast_t** astp)
{
  ast_t* ast = *astp;
  typecheck_t* t = &opt->check;

  if(!method_application(opt, ast, true))
    return false;

  AST_GET_CHILDREN(ast, positional, namedargs, lhs);

  // LHS must be a TK_TILDE, possibly contained in a TK_QUALIFY.
  AST_GET_CHILDREN(lhs, receiver, method);

  switch(ast_id(receiver))
  {
    case TK_NEWAPP:
    case TK_BEAPP:
    case TK_FUNAPP:
      AST_GET_CHILDREN_NO_DECL(receiver, receiver, method);
      break;

    default: {}
  }

  // The TK_FUNTYPE of the LHS.
  ast_t* type = ast_type(lhs);

  if(is_typecheck_error(type))
    return false;

  token_id apply_cap = partial_application_cap(type, receiver, positional);
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
      NONE));

  // We will have an apply method in the type.
  token_id can_error = ast_canerror(lhs) ? TK_QUESTION : TK_NONE;

  BUILD(apply, ast,
    NODE(TK_FUN, AST_SCOPE
      NODE(apply_cap)
      ID("apply")
      NONE
      NODE(TK_PARAMS)
      TREE(result)
      NODE(can_error)
      NODE(TK_SEQ)
      NONE));

  // We will replace partial application with $0.create(...)
  BUILD(call_receiver, ast, NODE(TK_REFERENCE, TREE(c_id)));
  BUILD(call_dot, ast, NODE(TK_DOT, TREE(call_receiver) ID("create")));

  BUILD(call, ast,
    NODE(TK_CALL,
      NONE
      NODE(TK_NAMEDARGS)
      TREE(call_dot)));

  ast_t* class_members = ast_childidx(def, 4);
  ast_t* create_params = ast_childidx(create, 3);
  ast_t* create_body = ast_childidx(create, 6);
  ast_t* apply_params = ast_childidx(apply, 3);
  ast_t* apply_body = ast_childidx(apply, 6);
  ast_t* call_namedargs = ast_childidx(call, 1);

  // Add the receiver to the anonymous type.
  ast_t* r_id = ast_from_string(receiver, package_hygienic_id(t));
  ast_t* r_field_id = ast_from_string(receiver, package_hygienic_id(t));
  ast_t* r_type = ast_type(receiver);

  if(is_typecheck_error(r_type))
    return false;

  // A field in the type.
  BUILD(r_field, receiver, NODE(TK_FLET, TREE(r_field_id) TREE(r_type) NONE));

  // A parameter of the constructor.
  BUILD(r_ctor_param, receiver, NODE(TK_PARAM, TREE(r_id) TREE(r_type) NONE));

  // An assignment in the constructor body.
  BUILD(r_assign, receiver,
    NODE(TK_ASSIGN,
      NODE(TK_CONSUME, NODE(TK_NONE) NODE(TK_REFERENCE, TREE(r_id)))
      NODE(TK_REFERENCE, TREE(r_field_id))));

  // A named argument at the call site.
  BUILD(r_call_seq, receiver, NODE(TK_SEQ, TREE(receiver)));
  BUILD(r_call_arg, receiver, NODE(TK_NAMEDARG, TREE(r_id) TREE(r_call_seq)));
  ast_settype(r_call_seq, r_type);

  ast_append(class_members, r_field);
  ast_append(create_params, r_ctor_param);
  ast_append(create_body, r_assign);
  ast_append(call_namedargs, r_call_arg);

  // Add a call to the original method to the apply body.
  BUILD(apply_call, ast,
    NODE(TK_CALL,
      NODE(TK_POSITIONALARGS)
      NONE
      NODE(TK_DOT, NODE(TK_REFERENCE, TREE(r_field_id)) TREE(method))));

  ast_append(apply_body, apply_call);
  ast_t* apply_args = ast_child(apply_call);

  // Add the arguments to the anonymous type.
  ast_t* arg = ast_child(positional);
  ast_t* param = ast_child(params);

  while(arg != NULL)
  {
    AST_GET_CHILDREN(param, id, p_type);

    if(ast_id(arg) == TK_NONE)
    {
      // A parameter of the apply method, using the same name, type and default
      // argument.
      ast_append(apply_params, param);

      // An arg in the call to the original method.
      BUILD(apply_arg, param,
        NODE(TK_SEQ,
          NODE(TK_CONSUME,
            NODE(TK_NONE)
            NODE(TK_REFERENCE, TREE(id)))));

      ast_append(apply_args, apply_arg);
    } else {
      ast_t* p_id = ast_from_string(id, package_hygienic_id(t));

      // A field in the type.
      BUILD(field, arg, NODE(TK_FLET, TREE(id) TREE(p_type) NONE));

      // A parameter of the constructor.
      BUILD(ctor_param, arg, NODE(TK_PARAM, TREE(p_id) TREE(p_type) NONE));

      // An assignment in the constructor body.
      BUILD(assign, arg,
        NODE(TK_ASSIGN,
          NODE(TK_CONSUME, NODE(TK_NONE) NODE(TK_REFERENCE, TREE(p_id)))
          NODE(TK_REFERENCE, TREE(id))));

      // A named argument at the call site.
      BUILD(call_arg, arg, NODE(TK_NAMEDARG, TREE(p_id) TREE(arg)));

      // An arg in the call to the original method.
      BUILD(apply_arg, arg, NODE(TK_SEQ, NODE(TK_REFERENCE, TREE(id))));

      ast_append(class_members, field);
      ast_append(create_params, ctor_param);
      ast_append(create_body, assign);
      ast_append(call_namedargs, call_arg);
      ast_append(apply_args, apply_arg);
    }

    arg = ast_sibling(arg);
    param = ast_sibling(param);
  }

  // Add create and apply to the anonymous type.
  ast_append(class_members, create);
  ast_append(class_members, apply);

  // Typecheck the anonymous type.
  ast_add(t->frame->module, def);

  if(!type_passes(def, opt))
    return false;

  // Typecheck the create call.
  if(!expr_reference(opt, &call_receiver))
    return false;

  if(!expr_dot(opt, &call_dot))
    return false;

  if(!expr_call(opt, &call))
    return false;

  // Replace the partial application with the create call.
  ast_replace(astp, call);
  return true;
}

bool expr_call(pass_opt_t* opt, ast_t** astp)
{
  ast_t* ast = *astp;

  if(!literal_call(ast, opt))
    return false;

  // Type already set by literal handler
  if(ast_type(ast) != NULL)
    return true;

  AST_GET_CHILDREN(ast, positional, namedargs, lhs);

  switch(ast_id(lhs))
  {
    case TK_NEWREF:
    case TK_BEREF:
    case TK_FUNREF:
      return method_call(opt, ast);

    case TK_NEWAPP:
    case TK_BEAPP:
    case TK_FUNAPP:
      return partial_application(opt, astp);

    default: {}
  }

  return insert_apply(opt, astp);
}
