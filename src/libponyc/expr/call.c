#include "call.h"
#include "ffi.h"
#include "infer.h"
#include "postfix.h"
#include "control.h"
#include "literal.h"
#include "operator.h"
#include "reference.h"
#include "../ast/astbuild.h"
#include "../ast/lexer.h"
#include "../pkg/package.h"
#include "../pass/expr.h"
#include "../pass/sugar.h"
#include "../type/alias.h"
#include "../type/cap.h"
#include "../type/assemble.h"
#include "../type/lookup.h"
#include "../type/reify.h"
#include "../type/safeto.h"
#include "../type/sanitise.h"
#include "../type/subtype.h"
#include "../type/typealias.h"
#include "../type/viewpoint.h"
#include "ponyassert.h"

static bool type_contains_thistype(ast_t* ast)
{
  if(ast == NULL)
    return false;

  if(ast_id(ast) == TK_THISTYPE)
    return true;

  for(ast_t* child = ast_child(ast); child != NULL; child = ast_sibling(child))
  {
    if(type_contains_thistype(child))
      return true;
  }

  return false;
}

static bool insert_apply(pass_opt_t* opt, ast_t** astp)
{
  // Sugar .apply()
  ast_t* ast = *astp;
  AST_GET_CHILDREN(ast, lhs, positional, namedargs, question);

  ast_t* dot = ast_from(ast, TK_DOT);
  ast_add(dot, ast_from_string(ast, "apply", opt->strtab));
  ast_swap(lhs, dot);
  ast_add(dot, lhs);

  if(!expr_dot(opt, &dot))
    return false;

  return expr_call(opt, astp);
}

bool method_check_type_params(pass_opt_t* opt, ast_t** astp)
{
  ast_t* lhs = *astp;
  ast_t* type = ast_type(lhs);

  if(is_typecheck_error(type))
    return false;

  ast_t* typeparams = ast_childidx(type, 1);
  pony_assert(ast_id(type) == TK_FUNTYPE);

  if(ast_id(typeparams) == TK_NONE)
    return true;

  BUILD(typeargs, ast_parent(lhs), NODE(TK_TYPEARGS));

  if(!reify_defaults(typeparams, typeargs, true, opt))
  {
    ast_free_unattached(typeargs);
    return false;
  }

  if(!check_constraints(lhs, typeparams, typeargs, true, opt))
  {
    ast_free_unattached(typeargs);
    return false;
  }

  type = reify(type, typeparams, typeargs, opt, true);
  typeparams = ast_childidx(type, 1);
  ast_replace(&typeparams, ast_from(typeparams, TK_NONE));

  REPLACE(astp, NODE(ast_id(lhs), TREE(lhs) TREE(typeargs)));
  ast_settype(*astp, type);

  return true;
}

bool apply_typeargs(pass_opt_t* opt, ast_t** astp, ast_t* typeargs,
  ast_t* inferred_from)
{
  ast_t* lhs = *astp;
  ast_t* type = ast_type(lhs);

  if(is_typecheck_error(type))
  {
    ast_free_unattached(typeargs);
    return false;
  }

  ast_t* typeparams = ast_childidx(type, 1);
  pony_assert(ast_id(type) == TK_FUNTYPE);

  if(inferred_from != NULL)
    ast_setdata(typeargs, inferred_from);

  if(!check_constraints(lhs, typeparams, typeargs, true, opt))
  {
    if(inferred_from != NULL)
    {
      ast_t* tp = ast_child(inferred_from);
      ast_t* ta = ast_child(typeargs);

      while(tp != NULL && ta != NULL)
      {
        if(ast_line(ta) != ast_line(lhs) || ast_pos(ta) != ast_pos(lhs))
        {
          ast_error_continue(opt->check.errors, ta,
            "'%s' was inferred as %s from this argument",
            ast_name(ast_child(tp)),
            ast_print_type(ta, opt->strtab));
        }

        tp = ast_sibling(tp);
        ta = ast_sibling(ta);
      }
    }

    ast_free_unattached(typeargs);
    return false;
  }

  type = reify(type, typeparams, typeargs, opt, true);
  typeparams = ast_childidx(type, 1);
  ast_replace(&typeparams, ast_from(typeparams, TK_NONE));

  REPLACE(astp, NODE(ast_id(lhs), TREE(lhs) TREE(typeargs)));
  ast_settype(*astp, type);

  return true;
}

static bool method_infer_type_params(pass_opt_t* opt, ast_t** astp,
  ast_t* positional, ast_t* namedargs)
{
  ast_t* lhs = *astp;
  ast_t* type = ast_type(lhs);

  if(is_typecheck_error(type))
    return false;

  pony_assert(ast_id(type) == TK_FUNTYPE);
  ast_t* typeparams = ast_childidx(type, 1);

  if(ast_id(typeparams) == TK_NONE)
    return true;

  AST_GET_CHILDREN(type, cap, tp_node, params, result);

  if(!normalise_args(opt, params, positional, namedargs))
    return false;

  ast_t* method_name = ast_childidx(lhs, 1);
  const char* callee_name = (method_name != NULL && ast_id(method_name) == TK_ID)
    ? ast_name(method_name) : "apply";

  ast_t* typeargs = infer_typeargs(opt, typeparams, NULL, params, positional,
    ast_parent(lhs), callee_name);

  if(typeargs == NULL)
    return false;

  return apply_typeargs(opt, astp, typeargs, typeparams);
}

static bool extend_positional_args(pass_opt_t* opt, ast_t* params,
  ast_t* positional)
{
  // Fill out the positional args to be as long as the param list.
  size_t param_len = ast_childcount(params);
  size_t arg_len = ast_childcount(positional);

  if(arg_len > param_len)
  {
    ast_error(opt->check.errors, positional, "too many arguments");
    ast_error_continue(opt->check.errors, params, "definition is here");
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

static bool apply_named_args(pass_opt_t* opt, ast_t* params, ast_t* positional,
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
        ast_error(opt->check.errors, arg_id,
          "cannot use sugar, update() has no parameter named \"value\"");
        return false;
      }

      ast_error(opt->check.errors, arg_id, "not a parameter name");
      return false;
    }

    ast_t* arg_replace = ast_childidx(positional, param_index);

    if(ast_id(arg_replace) != TK_NONE)
    {
      ast_error(opt->check.errors, arg_id,
        "named argument is already supplied");
      ast_error_continue(opt->check.errors, arg_replace,
        "supplied argument is here");
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

bool normalise_args(pass_opt_t* opt, ast_t* params, ast_t* positional,
  ast_t* namedargs)
{
  if(!extend_positional_args(opt, params, positional))
    return false;

  return apply_named_args(opt, params, positional, namedargs);
}

static bool apply_default_arg(pass_opt_t* opt, ast_t* param, ast_t** argp)
{
  // Pick up a default argument.
  AST_GET_CHILDREN(param, id, type, def_arg);

  if(ast_id(def_arg) == TK_NONE)
  {
    ast_error(opt->check.errors, *argp, "not enough arguments");
    ast_error_continue(opt->check.errors, param, "definition is here");
    return false;
  }

  pony_assert(ast_id(def_arg) == TK_SEQ);

  if(ast_id(ast_child(def_arg)) == TK_LOCATION)
  {
    // Default argument is __loc. Expand call location.
    ast_t* arg = *argp;
    ast_t* location = expand_location(arg, opt);
    ast_add(arg, location);
    ast_setid(arg, TK_SEQ);

    if(!ast_passes_subtree(&location, opt, PASS_EXPR))
      return false;
  }
  else
  {
    // Just use default argument.
    ast_replace(argp, def_arg);
  }

  if(!ast_passes_subtree(argp, opt, PASS_EXPR))
    return false;

  return true;
}

static ast_t* method_receiver_type(ast_t* method);

bool check_auto_recover_newref(ast_t* dest_type, ast_t* ast, pass_opt_t* opt)
{
  // we're not going to try auto-recovering to a complex type
  token_id dest_id = ast_id(dest_type);

  if(dest_id == TK_TYPEALIASREF)
  {
    ast_t* unfolded = typealias_unfold(dest_type);

    if(unfolded != NULL)
    {
      dest_id = ast_id(unfolded);
      ast_free_unattached(unfolded);
    }
  }

  if(dest_id != TK_NOMINAL)
    return false;

  while (ast != NULL && ast_id(ast) != TK_CALL)
    ast = ast_child(ast);

  if (ast == NULL)
    return false;

  AST_GET_CHILDREN(ast, newref, positional, named);
  if (ast_id(newref) != TK_NEWREF)
    return false;

  // sometimes for assignments there's a nested newref
  ast_t* child = ast_child(newref);
  if (child != NULL && ast_id(child) == TK_NEWREF)
    newref = child;

  ast_t* arg = ast_child(positional);
  while (arg != NULL)
  {
    ast_t* arg_type = ast_type(arg);
    if (is_typecheck_error(arg_type))
      return false;

    ast_t* arg_type_aliased = alias(arg_type, opt);
    bool ok = safe_to_autorecover(dest_type, arg_type_aliased, WRITE, opt);
    ast_free_unattached(arg_type_aliased);

    if (!ok)
      return false;

    arg = ast_sibling(arg);
  }

  return true;
}

static bool check_arg_types(pass_opt_t* opt, ast_t* params, ast_t* positional,
  bool partial, bool is_bare)
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
        if(!apply_default_arg(opt, param, &arg))
          return false;
      }
    }

    ast_t* p_type = ast_childidx(param, 1);

    if(!coerce_literals(&arg, p_type, opt))
      return false;

    coerce_tuple_to_target(opt, arg, p_type);

    if(jumps_away_no_value(opt, arg, "an argument"))
      return false;

    ast_t* arg_type = ast_type(arg);

    if(is_typecheck_error(arg_type))
      return false;

    errorframe_t info = NULL;
    ast_t* wp_type = consume_type(p_type, TK_NONE, false, opt);
    if((wp_type != NULL) && check_auto_recover_newref(wp_type, arg, opt))
    {
      token_id arg_cap = ast_id(cap_fetch(wp_type));
      ast_t* recovered_arg_type = recover_type(arg_type, arg_cap, opt);
      if (recovered_arg_type)
        arg_type = recovered_arg_type;
    }

    if(wp_type == NULL)
    {
      errorframe_t frame = NULL;
      ast_error_frame(&frame, arg, "argument not assignable to parameter");
      ast_error_frame(&frame, param, "parameter type is illegal: %s",
                      ast_print_type(p_type, opt->strtab));
      errorframe_append(&frame, &info);
      errorframe_report(&frame, opt->check.errors);

      return false;
    }
    else if(!is_subtype(arg_type, wp_type, &info, opt) && (!is_bare || (!void_star_param(wp_type, arg_type))))
    {
      errorframe_t frame = NULL;
      ast_error_frame(&frame, arg, "argument not assignable to parameter");
      ast_error_frame(&frame, arg, "argument type is %s",
                      ast_print_type(arg_type, opt->strtab));
      ast_error_frame(&frame, param, "parameter type requires %s",
                      ast_print_type(wp_type, opt->strtab));

      if (ast_childcount(arg) > 1)
        ast_error_frame(&frame, arg,
          "note that arguments must be separated by a comma");

      if(ast_checkflag(ast_type(arg), AST_FLAG_INCOMPLETE))
        ast_error_frame(&frame, arg,
          "this might be possible if all fields were already defined");

      // Only explain the 'this->' viewpoint when it is actually the cause: the
      // reified requirement still carries the arrow (so it genuinely depends on
      // the receiver), and the argument would be assignable if capabilities
      // were ignored (so the failure is about the capability, not the entity).
      if(type_contains_thistype(wp_type) &&
        is_subtype_ignore_cap(arg_type, wp_type, NULL, opt))
        ast_error_frame(&frame, param,
          "this parameter's type is adapted through the receiver's "
          "viewpoint (the 'this->' arrow), so the capability it requires "
          "depends on how the receiver is viewed, not just the capability "
          "written in the type");

      errorframe_append(&frame, &info);
      errorframe_report(&frame, opt->check.errors);
      ast_free_unattached(wp_type);
      return false;
    }

    ast_free_unattached(wp_type);
    arg = ast_sibling(arg);
    param = ast_sibling(param);
  }

  return true;
}

static bool auto_recover_call(ast_t* ast, ast_t* receiver_type, pass_opt_t* opt,
  ast_t* positional, ast_t* result)
{
  switch(ast_id(ast))
  {
    case TK_FUNREF:
    case TK_FUNAPP:
    case TK_FUNCHAIN:
      break;

    default:
      pony_assert(0);
      break;
  }

  // We can recover the receiver (ie not alias the receiver type) if all
  // arguments are safe and the result is either safe or unused.
  // The result of a chained method is always unused.
  ast_t* call = ast_parent(ast);
  if(is_result_needed(call, opt) && !safe_to_autorecover(receiver_type, result, EXTRACT, opt))
    return false;

  ast_t* arg = ast_child(positional);

  while(arg != NULL)
  {
    if(ast_id(arg) != TK_NONE)
    {
      ast_t* arg_type = ast_type(arg);

      if(is_typecheck_error(arg_type))
        return false;

      ast_t* a_type = alias(arg_type, opt);
      bool ok = safe_to_autorecover(receiver_type, a_type, WRITE, opt);
      ast_free_unattached(a_type);

      if(!ok)
        return false;
    }

    arg = ast_sibling(arg);
  }

  return true;
}

static ast_t* method_receiver(ast_t* method)
{
  ast_t* receiver = ast_child(method);

  // Dig through function qualification.
  if((ast_id(receiver) == TK_FUNREF) || (ast_id(receiver) == TK_FUNAPP) ||
     (ast_id(receiver) == TK_FUNCHAIN))
    receiver = ast_child(receiver);

  return receiver;
}

static ast_t* method_receiver_type(ast_t* method)
{
  ast_t* receiver = ast_child(method);

  // Dig through function qualification.
  if((ast_id(receiver) == TK_FUNREF) || (ast_id(receiver) == TK_FUNAPP) ||
     (ast_id(receiver) == TK_FUNCHAIN))
    receiver = ast_child(receiver);

  ast_t* r_type = ast_type(receiver);

  return r_type;
}

static bool check_receiver_cap(pass_opt_t* opt, ast_t* ast, bool* recovered)
{
  AST_GET_CHILDREN(ast, lhs, positional, namedargs, question);

  ast_t* type = ast_type(lhs);

  if(is_typecheck_error(type))
    return false;

  AST_GET_CHILDREN(type, cap, typeparams, params, result);

  // Receiver type, alias of receiver type, and target type.
  ast_t* r_type = method_receiver_type(lhs);

  if(is_typecheck_error(r_type))
    return false;

  ast_t* t_type = set_cap_and_ephemeral(r_type, ast_id(cap), TK_EPHEMERAL);
  ast_t* a_type;

  // If we can recover the receiver, we don't alias it here.
  bool can_recover = auto_recover_call(lhs, r_type, opt, positional, result);
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
      pony_assert(0);
  }

  if(can_recover && cap_recover)
  {
    a_type = unisolated(r_type);
    if(recovered != NULL)
      *recovered = true;
  }
  else
  {
    a_type = r_type;
    if(recovered != NULL)
      *recovered = false;
  }

  errorframe_t info = NULL;
  bool ok = is_subtype(a_type, t_type, &info, opt);

  if(!ok)
  {
    errorframe_t frame = NULL;

    ast_error_frame(&frame, ast,
      "receiver type is not a subtype of target type");

    switch (ast_id(a_type)) { // provide better information if the refcap is `this->*`
      case TK_ARROW:
        ast_error_frame(&frame, ast_child(lhs),
          "receiver type: %s (which becomes '%s' in this context)", ast_print_type(a_type, opt->strtab), ast_print_type(viewpoint_upper(a_type, opt), opt->strtab));
        break;

      default:
        ast_error_frame(&frame, ast_child(lhs),
          "receiver type: %s", ast_print_type(a_type, opt->strtab));
    }

    ast_error_frame(&frame, cap,
      "target type: %s", ast_print_type(t_type, opt->strtab));
    errorframe_append(&frame, &info);

    if(ast_checkflag(ast_type(method_receiver(lhs)), AST_FLAG_INCOMPLETE))
      ast_error_frame(&frame, method_receiver(lhs),
        "this might be possible if all fields were already defined");

    if(!can_recover && cap_recover && is_subtype(r_type, t_type, NULL, opt))
    {
      ast_error_frame(&frame, ast,
        "this would be possible if the arguments and return value "
        "were all sendable");
    }

    ast_t* fn = ast_nearest(lhs, TK_FUN);
    if (fn != NULL && ast_id(a_type) == TK_ARROW)
    {
      ast_t* iso = ast_child(fn);
      pony_assert(iso != NULL);
      token_id iso_id = ast_id(iso);

      ast_t* t_cap = cap_fetch(t_type);
      pony_assert(t_cap != NULL);

      if (ast_id(t_cap) == TK_REF && (iso_id == TK_BOX || iso_id == TK_VAL || iso_id == TK_TAG))
      {
        ast_error_frame(&frame, iso, "you are trying to change state in a %s function; this would be possible in a ref function", lexer_print(iso_id));
      }
    }

    errorframe_report(&frame, opt->check.errors);
  }

  if(a_type != r_type)
    ast_free_unattached(a_type);

  ast_free_unattached(r_type);
  ast_free_unattached(t_type);
  return ok;
}

static bool is_receiver_safe(typecheck_t* t, ast_t* ast, pass_opt_t* opt)
{
  switch(ast_id(ast))
  {
     case TK_THIS:
     case TK_FLETREF:
     case TK_FVARREF:
     case TK_EMBEDREF:
     case TK_PARAMREF:
     case TK_TUPLEELEMREF:
     {
       ast_t* type = ast_type(ast);
       return sendable(type, opt);
     }

     case TK_LETREF:
     case TK_VARREF:
     {
       ast_t* def = (ast_t*)ast_data(ast);
       pony_assert(def != NULL);
       ast_t* def_recover = ast_nearest(def, TK_RECOVER);
       if(t->frame->recover == def_recover)
         return true;
       ast_t* type = ast_type(ast);
       return sendable(type, opt);
     }

     default:
       // Unsafe receivers inside expressions are catched before we get there.
       return true;
  }
}

static bool check_nonsendable_recover(pass_opt_t* opt, ast_t* ast)
{
  if(opt->check.frame->recover != NULL)
  {
    AST_GET_CHILDREN(ast, lhs, positional, namedargs, question);

    ast_t* type = ast_type(lhs);

    AST_GET_CHILDREN(type, cap, typeparams, params, result);

    // If the method is tag, the call is always safe.
    if(ast_id(cap) == TK_TAG)
      return true;

    ast_t* receiver = ast_child(lhs);

    // Dig through function qualification.
    if((ast_id(receiver) == TK_FUNREF) || (ast_id(receiver) == TK_FUNAPP) ||
       (ast_id(receiver) == TK_FUNCHAIN))
      receiver = ast_child(receiver);

    if(!is_receiver_safe(&opt->check, receiver, opt))
    {
      ast_t* arg = ast_child(positional);
      bool args_sendable = true;
      while(arg != NULL)
      {
        if(ast_id(arg) != TK_NONE)
        {
          // Don't typecheck arg_type, this was already done in
          // auto_recover_call.
          ast_t* arg_type = ast_type(arg);
          if(!sendable(arg_type, opt))
          {
            args_sendable = false;
            break;
          }
        }
        arg = ast_sibling(arg);
      }
      if(!args_sendable || !sendable(result, opt))
      {
        ast_error(opt->check.errors, ast, "can't call method on non-sendable "
          "object inside of a recover expression");
        ast_error_continue(opt->check.errors, ast, "this would be possible if "
          "the arguments and return value were all sendable");
        return false;
      }
    }
  }
  return true;
}

static bool method_application(pass_opt_t* opt, ast_t* ast, bool partial)
{
  AST_GET_CHILDREN(ast, lhs, positional, namedargs, question);

  ast_t* type = ast_type(lhs);

  if(is_typecheck_error(type))
    return false;

  ast_t* typeparams_check = ast_childidx(type, 1);
  bool needs_inference = (ast_id(typeparams_check) == TK_TYPEPARAMS);

  if(needs_inference)
  {
    ast_t* params_check = ast_childidx(type, 2);
    bool has_bindable = false;
    ast_t* p = ast_child(params_check);

    while(p != NULL)
    {
      if(infer_bindable_position(ast_childidx(p, 1), typeparams_check))
      {
        has_bindable = true;
        break;
      }

      p = ast_sibling(p);
    }

    if(!has_bindable)
      needs_inference = false;
  }

  if(needs_inference)
  {
    if(!method_infer_type_params(opt, &lhs, positional, namedargs))
      return false;
  }
  else
  {
    if(!method_check_type_params(opt, &lhs))
      return false;
  }

  type = ast_type(lhs);

  if(is_typecheck_error(type))
    return false;

  AST_GET_CHILDREN(type, cap, typeparams, params, result);
  bool bare = (ast_id(cap) == TK_AT);

  if(!needs_inference)
  {
    if(!extend_positional_args(opt, params, positional))
      return false;

    if(!apply_named_args(opt, params, positional, namedargs))
      return false;
  }

  if(!check_arg_types(opt, params, positional, partial, bare))
    return false;

  switch(ast_id(lhs))
  {
    case TK_FUNREF:
    case TK_FUNAPP:
      if(!bare)
      {
        if(!check_receiver_cap(opt, ast, NULL))
          return false;

        if(!check_nonsendable_recover(opt, ast))
          return false;
      } else {
        ast_t* receiver = ast_child(lhs);

        // Dig through function qualification.
        if((ast_id(receiver) == TK_FUNREF) || (ast_id(receiver) == TK_FUNAPP) ||
           (ast_id(receiver) == TK_FUNCHAIN))
          receiver = ast_child(receiver);

        ast_t* recv_type = ast_type(receiver);
        if(!is_known(recv_type) && (ast_id(receiver) == TK_TYPEREF))
        {
          ast_error(opt->check.errors, lhs, "a bare method cannot be called on "
            "an abstract type reference");
          return false;
        }
      }

      break;

    default: {}
  }

  return true;
}

static bool method_call(pass_opt_t* opt, ast_t* ast)
{
  if(!method_application(opt, ast, false))
    return false;

  AST_GET_CHILDREN(ast, lhs, positional, namedargs, question);
  ast_t* type = ast_type(lhs);

  if(is_typecheck_error(type))
    return false;

  AST_GET_CHILDREN(type, cap, typeparams, params, result);
  ast_settype(ast, result);

  return true;
}

static token_id partial_application_cap(pass_opt_t* opt, ast_t* ftype,
  ast_t* receiver, ast_t* positional, bool is_constructor)
{
  // Check if the apply method in the generated object literal can accept a box
  // receiver. If not, it must be a ref receiver. It can accept a box receiver
  // if box->receiver <: lhs->receiver and box->arg <: lhs->param.
  AST_GET_CHILDREN(ftype, cap, typeparams, params, result);

  ast_t* type;
  ast_t* view_type;
  ast_t* need_type;
  bool ok;

  // A constructor has no stored receiver, so the receiver check does not apply.
  if(!is_constructor)
  {
    type = ast_type(receiver);
    view_type = viewpoint_type(ast_from(type, TK_BOX), type, opt);
    need_type = set_cap_and_ephemeral(type, ast_id(cap), TK_NONE);

    ok = is_subtype(view_type, need_type, NULL, opt);
    ast_free_unattached(view_type);
    ast_free_unattached(need_type);

    if(!ok)
      return TK_REF;
  }

  ast_t* param = ast_child(params);
  ast_t* arg = ast_child(positional);

  while(arg != NULL)
  {
    if(ast_id(arg) != TK_NONE)
    {
      type = ast_type(arg);
      view_type = viewpoint_type(ast_from(type, TK_BOX), type, opt);
      need_type = ast_childidx(param, 1);

      ok = is_subtype(view_type, need_type, NULL, opt);
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

// Build the call receiver AST for partial application from a receiver type.
// Handles TK_TYPEALIASREF (unfolds to the underlying nominal), TK_NOMINAL
// (with or without type arguments), and TK_TYPEPARAMREF.
// Returns false on error.
static bool build_partial_call_receiver(ast_t** out, ast_t* receiver_type,
  ast_t* ast, ast_t* method, bool bare, pass_opt_t* opt)
{
  ast_t* unfolded = NULL;
  const char* alias_name = NULL;
  ast_t* alias_type_args = NULL;

  if(ast_id(receiver_type) == TK_TYPEALIASREF)
  {
    // Save the alias name for the foreign-package fallback below.
    alias_name = ast_name(ast_child(receiver_type));

    ast_t* alias_targs = ast_childidx(receiver_type, 1);
    if((ast_id(alias_targs) == TK_TYPEARGS)
      && (ast_childcount(alias_targs) > 0))
    {
      alias_type_args = alias_targs;
    }

    unfolded = typealias_unfold(receiver_type);
    if(unfolded == NULL)
      return false;
    receiver_type = unfolded;
  }

  bool ok = true;

  switch(ast_id(receiver_type))
  {
    case TK_NOMINAL:
    {
      AST_GET_CHILDREN(receiver_type, recv_type_package, recv_type_name,
        recv_type_args);

      const char* recv_package_str = ast_name(recv_type_package);
      const char* recv_name_str = ast_name(recv_type_name);

      ast_t* module = ast_nearest(ast, TK_MODULE);
      ast_t* package = ast_parent(module);
      ast_t* pkg_id = package_id(package, opt);
      const char* pkg_str = ast_name(pkg_id);

      const char* pkg_alias = NULL;
      bool use_alias_name = false;

      if(recv_package_str != pkg_str)
      {
        if(alias_name != NULL)
        {
          // The unfolded type is from a foreign package the call site's
          // module may not import. Use the alias name (which is in scope)
          // rather than the unfolded nominal name (which may not be).
          use_alias_name = true;
          recv_name_str = alias_name;
        } else {
          pkg_alias = package_alias_from_id(module, recv_package_str, opt);
        }
      }

      ast_free_unattached(pkg_id);

      bool has_typeargs;
      if(use_alias_name)
        has_typeargs = (alias_type_args != NULL);
      else
        has_typeargs = (ast_id(recv_type_args) == TK_TYPEARGS)
          && (ast_childcount(recv_type_args) > 0);

      ast_t* type_ref = NULL;

      if(pkg_alias != NULL)
      {
        // `package.Type`
        BUILD_NO_DECL(type_ref, ast,
          NODE(TK_DOT,
            NODE(TK_REFERENCE, ID(pkg_alias))
            ID(recv_name_str)));
      } else {
        // `Type`
        BUILD_NO_DECL(type_ref, ast,
          NODE(TK_REFERENCE, ID(recv_name_str)));
      }

      if(has_typeargs)
      {
        ast_t* targs = use_alias_name ? alias_type_args : recv_type_args;

        // `Type[Args].f` or `package.Type[Args].f`
        BUILD_NO_DECL(*out, ast,
          NODE(TK_DOT,
            NODE(TK_QUALIFY,
              TREE(type_ref)
              TREE(ast_dup(targs)))
            TREE(method)));
      } else {
        // `Type.f` or `package.Type.f`
        BUILD_NO_DECL(*out, ast,
          NODE(TK_DOT,
            TREE(type_ref)
            TREE(method)));
      }

      break;
    }

    case TK_TYPEPARAMREF:
    {
      if(bare)
      {
        ast_error(opt->check.errors, ast, "a bare method cannot be called on "
          "an abstract type reference");
        ok = false;
        break;
      }

      const char* param_name = ast_name(ast_child(receiver_type));

      BUILD_NO_DECL(*out, ast,
        NODE(TK_DOT,
          NODE(TK_REFERENCE, ID(param_name))
          TREE(method)));

      break;
    }

    default:
    {
      ast_error(opt->check.errors, ast, "a bare method cannot be called on "
        "an abstract type reference");
      ok = false;
      break;
    }
  }

  if(unfolded != NULL)
    ast_free_unattached(unfolded);

  return ok;
}

// Sugar for partial application, which we convert to a lambda.
static bool partial_application(pass_opt_t* opt, ast_t** astp)
{
  /* Example that we refer to throughout this function.
   * ```pony
   * class C
   *   fun f[T](a: A, b: B = b_default): R
   *
   * let recv: T = ...
   * recv~f[T2](foo)
   * ```
   *
   * Partial call is converted to:
   * ```pony
   * {(b: B = b_default)($0 = recv, a = foo): R => $0.f[T2](a, consume b) }
   * ```
   */

  ast_t* ast = *astp;
  typecheck_t* t = &opt->check;

  if(!method_application(opt, ast, true))
    return false;

  AST_GET_CHILDREN(ast, lhs, positional, namedargs, question);

  // LHS must be an application, possibly wrapped in another application
  // if the method had type parameters for qualification.
  pony_assert(ast_id(lhs) == TK_FUNAPP || ast_id(lhs) == TK_BEAPP ||
    ast_id(lhs) == TK_NEWAPP);
  AST_GET_CHILDREN(lhs, receiver, method);
  ast_t* type_args = NULL;

  if(ast_id(receiver) == ast_id(lhs))
  {
    type_args = method;
    AST_GET_CHILDREN_NO_DECL(receiver, receiver, method);
  }

  // Look up the original method definition for this method call.
  deferred_reification_t* method_def = lookup(opt, lhs, ast_type(receiver),
    ast_name(method));
  ast_t* method_ast = method_def->ast;

  // The deferred reification doesn't own the underlying AST so we can free it
  // safely.
  deferred_reify_free(method_def);

  pony_assert(ast_id(method_ast) == TK_FUN || ast_id(method_ast) == TK_BE ||
    ast_id(method_ast) == TK_NEW);

  // The TK_FUNTYPE of the LHS.
  ast_t* type = ast_type(lhs);
  pony_assert(ast_id(type) == TK_FUNTYPE);

  if(is_typecheck_error(type))
    return false;

  AST_GET_CHILDREN(type, cap, type_params, target_params, result);

  bool bare = ast_id(cap) == TK_AT;

  token_id apply_cap = TK_AT;
  if(!bare)
    apply_cap = partial_application_cap(opt, type, receiver, positional,
      ast_id(method_ast) == TK_NEW);

  token_id can_error = ast_id(ast_childidx(method_ast, 5));
  const char* recv_name = package_hygienic_id(t, opt);

  // Build lambda expression.
  ast_t* call_receiver = NULL;
  if(bare)
  {
    ast_t* arg = ast_child(positional);
    while(arg != NULL)
    {
      if(ast_id(arg) != TK_NONE)
      {
        ast_error(opt->check.errors, arg, "the partial application of a bare "
          "method cannot take arguments");
        return false;
      }

      arg = ast_sibling(arg);
    }

    ast_t* receiver_type = ast_type(receiver);
    if(is_bare(receiver_type, opt))
    {
      // Partial application on a bare object, simply return the object itself.
      ast_replace(astp, receiver);
      return true;
    }

    if(!build_partial_call_receiver(&call_receiver, receiver_type, ast,
      method, bare, opt))
      return false;
  } else if(ast_id(receiver) == TK_TYPEREF) {
    ast_t* receiver_type = ast_type(receiver);

    if(!build_partial_call_receiver(&call_receiver, receiver_type, ast,
      method, bare, opt))
      return false;
  } else {
    // `$0.f`
    BUILD_NO_DECL(call_receiver, ast,
      NODE(TK_DOT,
        NODE(TK_REFERENCE, ID(recv_name))
        TREE(method)));
  }

  ast_t* captures = NULL;
  if(bare)
  {
    captures = ast_from(receiver, TK_NONE);
  } else if(ast_id(receiver) == TK_TYPEREF) {
    // Constructor partial application - no capture needed for the type.
    captures = ast_from(receiver, TK_LAMBDACAPTURES);
  } else {
    // Build captures. We always have at least one capture, for receiver.
    // Capture: `$0 = recv`
    BUILD_NO_DECL(captures, receiver,
      NODE(TK_LAMBDACAPTURES,
        NODE(TK_LAMBDACAPTURE,
          ID(recv_name)
          NONE  // Infer type.
          TREE(receiver))));
  }

  // Process arguments.
  ast_t* target_param = ast_child(target_params);
  ast_t* lambda_params = ast_from(target_params, TK_NONE);
  ast_t* lambda_call_args = ast_from(positional, TK_NONE);
  ast_t* given_arg = ast_child(positional);

  while(given_arg != NULL)
  {
    pony_assert(target_param != NULL);
    const char* target_p_name = ast_name(ast_child(target_param));

    if(ast_id(given_arg) == TK_NONE)
    {
      // This argument is not supplied already, must be a lambda parameter.
      // Like `b` in example above.
      // Build a new a new TK_PARAM node rather than copying the target one,
      // since the target has already been processed to expr pass, and we need
      // a clean one.
      AST_GET_CHILDREN(target_param, p_id, p_type, p_default);

      // Parameter: `b: B = b_default`
      BUILD(lambda_param, target_param,
        NODE(TK_PARAM,
          TREE(p_id)
          TREE(sanitise_type(p_type, opt))
          TREE(p_default)));

      // ISSUE-2480: the default argument was already typed at the method
      // definition and is a self-contained expression. Mark it PRESERVE so it
      // is used as-is rather than re-walked when this synthesized lambda is
      // lifted to an anonymous class. expr_lambda clears PRESERVE on the params
      // node, but that clear is non-recursive, so a flag on the default-arg
      // subtree survives and shields it from expr_object's ast_resetpass +
      // expr re-walk -- which would otherwise corrupt an already-typed
      // literal-operator default (e.g. `USize = -1` -> `(1).neg()`).
      ast_t* lambda_default = ast_childidx(lambda_param, 2);
      if(ast_id(lambda_default) != TK_NONE)
        ast_setflag(lambda_default, AST_FLAG_PRESERVE);

      ast_append(lambda_params, lambda_param);
      ast_setid(lambda_params, TK_PARAMS);

      // Argument: `consume b`
      BUILD(target_arg, lambda_param,
        NODE(TK_SEQ,
          NODE(TK_CONSUME,
            NONE
            NODE(TK_REFERENCE, ID(target_p_name)))));

      ast_append(lambda_call_args, target_arg);
      ast_setid(lambda_call_args, TK_POSITIONALARGS);
    }
    else
    {
      // This argument is supplied to the partial, capture it.
      // Like `a` in example above.
      // Capture: `a = foo`
      BUILD(capture, given_arg,
        NODE(TK_LAMBDACAPTURE,
          ID(target_p_name)
          NONE
          TREE(given_arg)));

      ast_append(captures, capture);

      // Argument: `a`
      BUILD(target_arg, given_arg,
        NODE(TK_SEQ,
          NODE(TK_REFERENCE, ID(target_p_name))));

      ast_append(lambda_call_args, target_arg);
      ast_setid(lambda_call_args, TK_POSITIONALARGS);
    }

    given_arg = ast_sibling(given_arg);
    target_param = ast_sibling(target_param);
  }

  pony_assert(target_param == NULL);

  if(type_args != NULL)
  {
    // The partial call has type args, add them to the actual call in apply().
    // `$0.f[T2]`
    BUILD(qualified, type_args,
      NODE(TK_QUALIFY,
        TREE(call_receiver)
        TREE(type_args)));
    call_receiver = qualified;
  }

  REPLACE(astp,
    NODE((bare ? TK_BARELAMBDA : TK_LAMBDA),
      NODE(apply_cap)
      NONE  // Lambda function name.
      NONE  // Lambda type params.
      TREE(lambda_params)
      TREE(captures)
      TREE(sanitise_type(result, opt))
      NODE(can_error)
      NODE(TK_SEQ,
        NODE(TK_CALL,
          TREE(call_receiver)
          TREE(lambda_call_args)
          NONE  // Named args.
          NODE(can_error)))
      NONE)); // Lambda reference capability.

  // Need to preserve various lambda children.
  ast_setflag(ast_childidx(*astp, 2), AST_FLAG_PRESERVE); // Type params.
  ast_setflag(ast_childidx(*astp, 3), AST_FLAG_PRESERVE); // Parameters.
  ast_setflag(ast_childidx(*astp, 5), AST_FLAG_PRESERVE); // Return type.
  ast_setflag(ast_childidx(*astp, 7), AST_FLAG_PRESERVE); // Body.

  // Catch up to this pass.
  return ast_passes_subtree(astp, opt, PASS_EXPR);
}

static bool method_chain(pass_opt_t* opt, ast_t* ast)
{
  if(!method_application(opt, ast, false))
    return false;

  AST_GET_CHILDREN(ast, lhs, positional, namedargs, question);

  ast_t* type = ast_type(lhs);
  if(ast_id(ast_child(type)) == TK_AT)
  {
    ast_error(opt->check.errors, ast, "a bare method cannot be chained");
    return false;
  }

  // We check the receiver cap now instead of in method_application because
  // we need to know whether the receiver was recovered.
  ast_t* r_type = method_receiver_type(lhs);
  if(ast_id(lhs) == TK_FUNCHAIN)
  {
    bool recovered;
    if(!check_receiver_cap(opt, ast, &recovered))
      return false;

    if(!check_nonsendable_recover(opt, ast))
      return false;

    ast_t* f_type = ast_type(lhs);
    token_id f_cap = ast_id(ast_child(f_type));

    ast_t* c_type = chain_type(r_type, f_cap, recovered, opt);
    ast_settype(ast, c_type);
  } else {
    ast_settype(ast, r_type);
  }

  return true;
}

static ast_t* constructor_path_typeref(ast_t* lhs, const char** member_name)
{
  token_id lid = ast_id(lhs);

  if(lid == TK_TYPEREF)
  {
    *member_name = "create";
    return lhs;
  }

  if(lid == TK_DOT || lid == TK_TILDE)
  {
    ast_t* receiver = ast_child(lhs);

    if(ast_id(receiver) == TK_TYPEREF)
    {
      ast_t* name_node = ast_sibling(receiver);

      if(name_node != NULL && ast_id(name_node) == TK_ID)
        *member_name = ast_name(name_node);
      else
        *member_name = "create";

      return receiver;
    }
  }

  if(lid == TK_QUALIFY)
  {
    ast_t* inner = ast_child(lhs);
    token_id iid = ast_id(inner);

    if(iid == TK_DOT || iid == TK_TILDE)
    {
      ast_t* receiver = ast_child(inner);

      if(ast_id(receiver) == TK_TYPEREF)
      {
        ast_t* name_node = ast_sibling(receiver);

        if(name_node != NULL && ast_id(name_node) == TK_ID)
          *member_name = ast_name(name_node);
        else
          *member_name = "create";

        return receiver;
      }
    }
  }

  return NULL;
}

static ast_result_t constructor_path_infer(pass_opt_t* opt, ast_t* ast,
  ast_t* typeref, const char* member_name)
{
  ast_t* typeargs = ast_childidx(typeref, 2);

  if(ast_id(typeargs) != TK_NONE)
    return AST_OK;

  ast_t* def = (ast_t*)ast_data(typeref);

  if(def == NULL)
    return AST_OK;

  switch(ast_id(def))
  {
    case TK_PRIMITIVE:
    case TK_STRUCT:
    case TK_CLASS:
    case TK_ACTOR:
      break;

    default:
      return AST_OK;
  }

  ast_t* class_typeparams = ast_childidx(def, 1);

  if(ast_id(class_typeparams) != TK_TYPEPARAMS)
    return AST_OK;

  ast_t* member = ast_get(def, stringtab(opt->strtab, member_name), NULL);

  if(member == NULL || ast_id(member) != TK_NEW)
    return AST_OK;

  ast_t* method_typeparams = ast_childidx(member, 2);
  ast_t* params = ast_childidx(member, 3);

  if(ast_id(params) == TK_NONE || ast_childcount(params) == 0)
    return AST_OK;

  bool has_bindable = false;
  ast_t* p = ast_child(params);

  while(p != NULL)
  {
    if(infer_bindable_position(ast_childidx(p, 1), class_typeparams))
    {
      has_bindable = true;
      break;
    }

    p = ast_sibling(p);
  }

  if(!has_bindable)
    return AST_OK;

  ast_t* positional = ast_childidx(ast, 1);
  ast_t* namedargs = ast_childidx(ast, 2);

  if(!normalise_args(opt, params, positional, namedargs))
    return AST_ERROR;

  // Visit arguments early. Skip arguments that contain dependent
  // expressions without explicit types (untyped lambdas, untyped arrays)
  // at positions whose parameter type mentions a type parameter — they
  // will be typed by the normal post-order visit against the reified
  // parameter type.
  ast_t* param = ast_child(params);
  ast_t* arg = ast_child(positional);

  while(param != NULL && arg != NULL)
  {
    if(ast_id(arg) != TK_NONE)
    {
      bool skip = false;

      if(infer_needs_antecedent_type(arg))
      {
        ast_t* p_type = ast_childidx(param, 1);

        if(type_mentions_typeparams(p_type, class_typeparams))
          skip = true;
      }

      if(!skip)
      {
        ast_result_t ar = ast_visit(&arg, pass_pre_expr, pass_expr,
          opt, PASS_EXPR);

        if(ar == AST_FATAL)
          return AST_FATAL;
      }
    }

    param = ast_sibling(param);
    arg = ast_sibling(arg);
  }

  positional = ast_childidx(ast, 1);

  ast_t* other_tp = (ast_id(method_typeparams) == TK_TYPEPARAMS)
    ? method_typeparams : NULL;

  ast_t* inferred = infer_typeargs(opt, class_typeparams, other_tp, params,
    positional, typeref, member_name);

  if(inferred == NULL)
  {
    ast_settype(ast, ast_from(ast, TK_ERRORTYPE));
    return AST_ERROR;
  }

  if(!check_constraints(typeref, class_typeparams, inferred, true, opt))
  {
    ast_free_unattached(inferred);
    ast_settype(ast, ast_from(ast, TK_ERRORTYPE));
    return AST_ERROR;
  }

  ast_replace(&typeargs, inferred);
  ast_setdata(inferred, (void*)class_typeparams);

  ast_t* ta_child = ast_child(inferred);
  while(ta_child != NULL)
  {
    ast_pass_record(ta_child, PASS_EXPR);
    ta_child = ast_sibling(ta_child);
  }
  ast_pass_record(inferred, PASS_EXPR);

  return AST_OK;
}

ast_result_t expr_pre_call(pass_opt_t* opt, ast_t** astp)
{
  ast_t* ast = *astp;
  ast_t* lhs = ast_child(ast);

  // Constructor-path shapes: the LHS is rooted at an unqualified TYPEREF.
  const char* member_name = NULL;
  ast_t* typeref = constructor_path_typeref(lhs, &member_name);

  if(typeref != NULL)
  {
    ast_result_t cr = constructor_path_infer(opt, ast, typeref, member_name);

    if(cr != AST_OK)
      return cr;
  }

  // Type the LHS.
  ast_result_t r = ast_visit(&lhs, pass_pre_expr, pass_expr, opt, PASS_EXPR);

  // Re-read in case expr_dot/expr_tilde replaced the child.
  lhs = ast_child(ast);

  if(r == AST_FATAL)
    return AST_FATAL;

  if(r == AST_ERROR || is_typecheck_error(ast_type(lhs)))
    return AST_ERROR;

  ast_t* type = ast_type(lhs);

  if(type == NULL || ast_id(type) != TK_FUNTYPE)
    return AST_OK;

  ast_t* typeparams = ast_childidx(type, 1);

  if(ast_id(typeparams) != TK_TYPEPARAMS)
    return AST_OK;

  // Check that at least one parameter mentions a type parameter.
  ast_t* params = ast_childidx(type, 2);
  bool has_bindable = false;
  ast_t* p = ast_child(params);

  while(p != NULL)
  {
    if(infer_bindable_position(ast_childidx(p, 1), typeparams))
    {
      has_bindable = true;
      break;
    }

    p = ast_sibling(p);
  }

  if(!has_bindable)
    return AST_OK;

  // Normalise arguments.
  ast_t* positional = ast_childidx(ast, 1);
  ast_t* namedargs = ast_childidx(ast, 2);

  if(!normalise_args(opt, params, positional, namedargs))
    return AST_ERROR;

  // Visit arguments early. Skip arguments that contain dependent
  // expressions without explicit types (untyped lambdas, untyped arrays)
  // at positions whose parameter type mentions a type parameter — they
  // will be typed by the normal post-order visit against the reified
  // parameter type.
  ast_t* param = ast_child(params);
  ast_t* arg = ast_child(positional);

  while(param != NULL && arg != NULL)
  {
    if(ast_id(arg) != TK_NONE)
    {
      bool skip = false;

      if(infer_needs_antecedent_type(arg))
      {
        ast_t* p_type = ast_childidx(param, 1);

        if(type_mentions_typeparams(p_type, typeparams))
          skip = true;
      }

      if(!skip)
      {
        ast_result_t ar = ast_visit(&arg, pass_pre_expr, pass_expr,
          opt, PASS_EXPR);

        if(ar == AST_FATAL)
          return AST_FATAL;
      }
    }

    param = ast_sibling(param);
    arg = ast_sibling(arg);
  }

  // Re-read after visits may have replaced children.
  positional = ast_childidx(ast, 1);
  lhs = ast_child(ast);

  ast_t* method_name = ast_childidx(lhs, 1);
  const char* callee_name =
    (method_name != NULL && ast_id(method_name) == TK_ID)
    ? ast_name(method_name) : "apply";

  ast_t* typeargs = infer_typeargs(opt, typeparams, NULL, params, positional,
    ast_parent(lhs), callee_name);

  if(typeargs == NULL)
  {
    ast_settype(ast, ast_from(ast, TK_ERRORTYPE));
    return AST_ERROR;
  }

  // &lhs has a parent, so REPLACE updates the tree.
  if(!apply_typeargs(opt, &lhs, typeargs, typeparams))
  {
    ast_settype(ast, ast_from(ast, TK_ERRORTYPE));
    return AST_ERROR;
  }

  return AST_OK;
}

bool expr_call(pass_opt_t* opt, ast_t** astp)
{
  ast_t* ast = *astp;

  if(!literal_call(ast, opt))
    return false;

  // Type already set by literal handler. Check for infertype, which is a
  // marker for typechecking default arguments.
  ast_t* type = ast_type(ast);

  if((type != NULL) && (ast_id(type) != TK_INFERTYPE))
    return true;

  AST_GET_CHILDREN(ast, lhs, positional, namedargs, question);

  switch(ast_id(lhs))
  {
    case TK_NEWREF:
    case TK_NEWBEREF:
    case TK_BEREF:
    case TK_FUNREF:
      return method_call(opt, ast);

    case TK_NEWAPP:
    case TK_BEAPP:
    case TK_FUNAPP:
      return partial_application(opt, astp);

    case TK_BECHAIN:
    case TK_FUNCHAIN:
      return method_chain(opt, ast);

    default: {}
  }

  return insert_apply(opt, astp);
}
