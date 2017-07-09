#include "reference.h"
#include "literal.h"
#include "postfix.h"
#include "call.h"
#include "../pass/expr.h"
#include "../pass/flatten.h"
#include "../pass/names.h"
#include "../pass/refer.h"
#include "../type/subtype.h"
#include "../type/assemble.h"
#include "../type/alias.h"
#include "../type/viewpoint.h"
#include "../type/cap.h"
#include "../type/reify.h"
#include "../type/lookup.h"
#include "../type/typeparam.h"
#include "../ast/astbuild.h"
#include "ponyassert.h"

static bool check_provides(pass_opt_t* opt, ast_t* type, ast_t* provides,
  errorframe_t* errorf)
{
  bool ok = true;

  switch(ast_id(provides))
  {
    case TK_NONE:
      return true;

    case TK_PROVIDES:
    case TK_ISECTTYPE:
    {
      for(ast_t* child = ast_child(provides);
        child != NULL;
        child = ast_sibling(child))
      {
        ok = check_provides(opt, type, child, errorf) && ok;
      }

      return ok;
    }

    case TK_NOMINAL:
      return is_sub_provides(type, provides, errorf, opt);

    default: {}
  }

  pony_assert(0);
  return false;
}

bool expr_provides(pass_opt_t* opt, ast_t* ast)
{
  // Check that the type actually provides everything it declares.
  // Since the traits pass has completed, all method imports have already
  // happened. At this point, we need to check that the type is a structural
  // subtype of all traits and interfaces it declares as provided.
  AST_GET_CHILDREN(ast, id, typeparams, cap, provides);

  ast_t* def = opt->check.frame->type;
  ast_t* type = type_for_class(opt, def, ast, TK_REF, TK_NONE, true);
  errorframe_t err = NULL;

  if(!check_provides(opt, type, provides, &err))
  {
    errorframe_t err2 = NULL;
    ast_error_frame(&err2, ast, "type does not implement its provides list");
    errorframe_append(&err2, &err);
    errorframe_report(&err2, opt->check.errors);
    return false;
  }

  return true;
}

bool expr_param(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, id, type, init);
  ast_settype(ast, type);
  bool ok = true;

  if(ast_id(init) != TK_NONE)
  {
    // Initialiser type must match declared type.
    if(!coerce_literals(&init, type, opt))
      return false;

    ast_t* init_type = ast_type(init);

    if(is_typecheck_error(init_type))
      return false;

    init_type = alias(init_type);
    errorframe_t err = NULL;

    if(!is_subtype(init_type, type, &err, opt))
    {
      errorframe_t err2 = NULL;
      ast_error_frame(&err2, init,
        "default argument is not a subtype of the parameter type");
      errorframe_append(&err2, &err);
      errorframe_report(&err2, opt->check.errors);
      ok = false;
    }

    ast_free_unattached(init_type);
  }

  return ok;
}

bool expr_field(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  AST_GET_CHILDREN(ast, id, type, init);
  ast_settype(ast, type);
  return true;
}

bool expr_fieldref(pass_opt_t* opt, ast_t* ast, ast_t* find, token_id tid)
{
  AST_GET_CHILDREN(ast, left, right);
  ast_t* l_type = ast_type(left);

  if(is_typecheck_error(l_type))
    return false;

  AST_GET_CHILDREN(find, id, f_type, init);

  f_type = typeparam_current(opt, f_type, ast);

  // Viewpoint adapted type of the field.
  ast_t* type = viewpoint_type(l_type, f_type);

  if(ast_id(type) == TK_ARROW)
  {
    ast_t* upper = viewpoint_upper(type);

    if(upper == NULL)
    {
      ast_error(opt->check.errors, ast, "can't read a field through %s",
        ast_print_type(l_type));
      return false;
    }

    ast_free_unattached(upper);
  }

  // In a recover expression, we can access obj.field if field is sendable
  // and not being assigned to, even if obj isn't sendable.

  if(opt->check.frame->recover != NULL)
  {
    if(!sendable(type))
    {
      if(!sendable(l_type))
      {
        errorframe_t frame = NULL;
        ast_error_frame(&frame, ast, "can't access field of non-sendable "
          "object inside of a recover expression");
        ast_error_frame(&frame, find, "this would be possible if the field was "
          "sendable");
        errorframe_report(&frame, opt->check.errors);
        return false;
      }
    }
    else
    {
      ast_t* parent = ast_parent(ast);
      ast_t* current = ast;
      while(ast_id(parent) != TK_RECOVER && ast_id(parent) != TK_ASSIGN)
      {
        current = parent;
        parent = ast_parent(parent);
      }
      if(ast_id(parent) == TK_ASSIGN && ast_child(parent) != current)
      {
        errorframe_t frame = NULL;
        ast_error_frame(&frame, ast, "can't access field of non-sendable "
          "object inside of a recover expression");
        ast_error_frame(&frame, parent, "this would be possible if the field "
          "wasn't assigned to");
        errorframe_report(&frame, opt->check.errors);
        return false;
      }
    }
  }

  // Set the unadapted field type.
  ast_settype(right, f_type);

  // Set the type so that it isn't free'd as unattached.
  ast_setid(ast, tid);
  ast_settype(ast, type);

  return true;
}

bool expr_typeref(pass_opt_t* opt, ast_t** astp)
{
  ast_t* ast = *astp;
  pony_assert(ast_id(ast) == TK_TYPEREF);
  AST_GET_CHILDREN(ast, package, id, typeargs);

  ast_t* type = ast_type(ast);
  if(type == NULL || (ast_id(type) == TK_INFERTYPE))
  {
    // Assemble the type node from the package name and type name strings.
    const char* name = ast_name(id);
    const char* package_name =
      (ast_id(package) != TK_NONE) ? ast_name(ast_child(package)) : NULL;
    type = type_sugar_args(ast, package_name, name, typeargs);
    ast_settype(ast, type);

    if(is_typecheck_error(type))
      return false;

    // Has to be valid.
    if(!expr_nominal(opt, &type))
    {
      ast_settype(ast, ast_from(type, TK_ERRORTYPE));
      ast_free_unattached(type);
      return false;
    }
  }

  switch(ast_id(ast_parent(ast)))
  {
    case TK_QUALIFY:
    case TK_DOT:
    case TK_TILDE:
    case TK_CHAIN:
      break;

    case TK_CALL:
    {
      ast_t* call = ast_parent(ast);

      // Transform to a default constructor.
      ast_t* dot = ast_from(ast, TK_DOT);
      ast_add(dot, ast_from_string(ast, "create"));
      ast_swap(ast, dot);
      *astp = dot;
      ast_add(dot, ast);

      if(!expr_dot(opt, astp))
      {
        ast_settype(ast, ast_from(type, TK_ERRORTYPE));
        ast_free_unattached(type);
        return false;
      }

      ast_t* ast = *astp;

      // If the default constructor has no parameters, transform to an apply
      // call.
      if((ast_id(ast) == TK_NEWREF) || (ast_id(ast) == TK_NEWBEREF))
      {
        type = ast_type(ast);

        if(is_typecheck_error(type))
          return false;

        pony_assert(ast_id(type) == TK_FUNTYPE);
        AST_GET_CHILDREN(type, cap, typeparams, params, result);

        if((ast_id(params) == TK_NONE) &&
          !ast_checkflag(call, AST_FLAG_INCOMPLETE))
        {
          // Add a call node.
          ast_t* call = ast_from(ast, TK_CALL);
          ast_add(call, ast_from(call, TK_NONE)); // Call partiality
          ast_add(call, ast_from(call, TK_NONE)); // Named
          ast_add(call, ast_from(call, TK_NONE)); // Positional
          ast_swap(ast, call);
          ast_append(call, ast);

          if(!expr_call(opt, &call))
          {
            ast_settype(ast, ast_from(type, TK_ERRORTYPE));
            ast_free_unattached(type);
            return false;
          }

          // Add a dot node.
          ast_t* apply = ast_from(call, TK_DOT);
          ast_add(apply, ast_from_string(call, "apply"));
          ast_swap(call, apply);
          ast_add(apply, call);

          if(!expr_dot(opt, &apply))
          {
            ast_settype(ast, ast_from(type, TK_ERRORTYPE));
            ast_free_unattached(type);
            return false;
          }
        }
      }

      return true;
    }

    default:
    {
      // Transform to a default constructor.
      ast_t* dot = ast_from(ast, TK_DOT);
      ast_add(dot, ast_from_string(ast, "create"));
      ast_swap(ast, dot);
      ast_add(dot, ast);

      // Call the default constructor with no arguments.
      ast_t* call = ast_from(ast, TK_CALL);
      ast_swap(dot, call);
      ast_add(call, dot); // Receiver comes last.
      ast_add(call, ast_from(ast, TK_NONE)); // Call partiality.
      ast_add(call, ast_from(ast, TK_NONE)); // Named args.
      ast_add(call, ast_from(ast, TK_NONE)); // Positional args.

      *astp = call;

      if(!expr_dot(opt, &dot))
      {
        ast_settype(ast, ast_from(type, TK_ERRORTYPE));
        ast_free_unattached(type);
        return false;
      }

      if(!expr_call(opt, astp))
      {
        ast_settype(ast, ast_from(type, TK_ERRORTYPE));
        ast_free_unattached(type);
        return false;
      }
      break;
    }
  }

  return true;
}

bool expr_dontcareref(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  pony_assert(ast_id(ast) == TK_DONTCAREREF);

  ast_settype(ast, ast_from(ast, TK_DONTCARETYPE));

  return true;
}

bool expr_local(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  pony_assert(ast_type(ast) != NULL);

  return true;
}

bool expr_localref(pass_opt_t* opt, ast_t* ast)
{
  pony_assert((ast_id(ast) == TK_VARREF) || (ast_id(ast) == TK_LETREF));

  ast_t* def = (ast_t*)ast_data(ast);
  pony_assert(def != NULL);

  ast_t* type = ast_type(def);

  if(type != NULL && ast_id(type) == TK_INFERTYPE)
  {
    ast_error(opt->check.errors, ast, "cannot infer type of %s\n",
      ast_nice_name(ast_child(def)));
    ast_settype(def, ast_from(def, TK_ERRORTYPE));
    ast_settype(ast, ast_from(ast, TK_ERRORTYPE));
    return false;
  }

  if(is_typecheck_error(type))
    return false;

  type = typeparam_current(opt, type, ast);

  if(!sendable(type))
  {
    if(opt->check.frame->recover != NULL)
    {
      ast_t* def_recover = ast_nearest(def, TK_RECOVER);

      if(opt->check.frame->recover != def_recover)
      {
        ast_t* parent = ast_parent(ast);
        if((ast_id(parent) != TK_DOT) && (ast_id(parent) != TK_CHAIN))
          type = set_cap_and_ephemeral(type, TK_TAG, TK_NONE);

        if(ast_id(ast) == TK_VARREF)
        {
          ast_t* current = ast;
          while(ast_id(parent) != TK_RECOVER && ast_id(parent) != TK_ASSIGN)
          {
            current = parent;
            parent = ast_parent(parent);
          }
          if(ast_id(parent) == TK_ASSIGN && ast_child(parent) != current)
          {
            ast_error(opt->check.errors, ast, "can't access a non-sendable "
              "local defined outside of a recover expression from within "
              "that recover epression");
            ast_error_continue(opt->check.errors, parent, "this would be "
              "possible if the local wasn't assigned to");
            return false;
          }
        }
      }
    }
  }

  // Get the type of the local and attach it to our reference.
  // Automatically consume a local if the function is done.
  ast_t* r_type = type;

  if(is_method_return(&opt->check, ast))
    r_type = consume_type(type, TK_NONE);

  ast_settype(ast, r_type);
  return true;
}

bool expr_paramref(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_PARAMREF);

  ast_t* def = (ast_t*)ast_data(ast);
  pony_assert(def != NULL);

  ast_t* type = ast_type(def);

  if(is_typecheck_error(type))
    return false;

  type = typeparam_current(opt, type, ast);

  if(!sendable(type) && (opt->check.frame->recover != NULL))
  {
    ast_t* parent = ast_parent(ast);
    if((ast_id(parent) != TK_DOT) && (ast_id(parent) != TK_CHAIN))
      type = set_cap_and_ephemeral(type, TK_TAG, TK_NONE);
  }

  // Get the type of the parameter and attach it to our reference.
  // Automatically consume a parameter if the function is done.
  ast_t* r_type = type;

  if(is_method_return(&opt->check, ast))
    r_type = consume_type(type, TK_NONE);

  ast_settype(ast, r_type);
  return true;
}

bool expr_addressof(pass_opt_t* opt, ast_t* ast)
{
  // Check if we're in an FFI call.
  ast_t* parent = ast_parent(ast);
  bool ok = false;

  if(ast_id(parent) == TK_SEQ)
  {
    parent = ast_parent(parent);

    if(ast_id(parent) == TK_POSITIONALARGS)
    {
      parent = ast_parent(parent);

      if(ast_id(parent) == TK_FFICALL)
        ok = true;
    }
  }

  if(!ok)
  {
    ast_error(opt->check.errors, ast,
      "the addressof operator can only be used for FFI arguments");
    return false;
  }

  ast_t* expr = ast_child(ast);

  switch(ast_id(expr))
  {
    case TK_FVARREF:
    case TK_VARREF:
    case TK_FUNREF:
    case TK_BEREF:
      break;

    case TK_FLETREF:
      ast_error(opt->check.errors, ast,
        "can't take the address of a let field");
      return false;

    case TK_EMBEDREF:
      ast_error(opt->check.errors, ast,
        "can't take the address of an embed field");
      return false;

    case TK_TUPLEELEMREF:
      ast_error(opt->check.errors, ast,
        "can't take the address of a tuple element");
      return false;

    case TK_LETREF:
      ast_error(opt->check.errors, ast,
        "can't take the address of a let local");
      return false;

    case TK_PARAMREF:
      ast_error(opt->check.errors, ast,
        "can't take the address of a function parameter");
      return false;

    default:
      ast_error(opt->check.errors, ast,
        "can only take the address of a local, field or method");
      return false;
  }

  ast_t* expr_type = ast_type(expr);

  if(is_typecheck_error(expr_type))
    return false;

  ast_t* type = NULL;

  switch(ast_id(expr))
  {
    case TK_FUNREF:
    case TK_BEREF:
    {
      if(!method_check_type_params(opt, &expr))
        return false;

      AST_GET_CHILDREN(expr, receiver, method);
      if(ast_id(receiver) == ast_id(expr))
        AST_GET_CHILDREN_NO_DECL(receiver, receiver, method);

      ast_t* def = lookup(opt, expr, ast_type(receiver), ast_name(method));
      pony_assert((ast_id(def) == TK_FUN) || (ast_id(def) == TK_BE));

      // Set the type to a bare lambda type equivalent to the function type.
      bool bare = ast_id(ast_child(def)) == TK_AT;
      ast_t* params = ast_childidx(def, 3);
      ast_t* result = ast_sibling(params);
      ast_t* partial = ast_sibling(result);

      ast_t* lambdatype_params = ast_from(params, TK_NONE);
      if(ast_id(params) != TK_NONE)
      {
        ast_setid(lambdatype_params, TK_PARAMS);
        ast_t* param = ast_child(params);
        while(param != NULL)
        {
          ast_t* param_type = ast_childidx(param, 1);
          ast_append(lambdatype_params, param_type);
          param = ast_sibling(param);
        }
      }

      if(!bare)
      {
        ast_setid(lambdatype_params, TK_PARAMS);
        ast_t* receiver_type = ast_type(receiver);
        ast_add(lambdatype_params, receiver_type);
      }

      BUILD_NO_DECL(type, expr_type,
        NODE(TK_BARELAMBDATYPE,
          NONE // receiver cap
          NONE // id
          NONE // type parameters
          TREE(lambdatype_params)
          TREE(result)
          TREE(partial)
          NODE(TK_VAL) // object cap
          NONE)); // object cap mod

      if(!ast_passes_subtree(&type, opt, PASS_EXPR))
        return false;

      break;
    }

    default:
      // Set the type to Pointer[ast_type(expr)].
      type = type_pointer_to(opt, expr_type);
      break;
  }

  ast_settype(ast, type);
  return true;
}

bool expr_digestof(pass_opt_t* opt, ast_t* ast)
{
  ast_t* expr = ast_child(ast);

  switch(ast_id(expr))
  {
    case TK_FVARREF:
    case TK_FLETREF:
    case TK_EMBEDREF:
    case TK_TUPLEELEMREF:
    case TK_VARREF:
    case TK_LETREF:
    case TK_PARAMREF:
    case TK_THIS:
      break;

    default:
      ast_error(opt->check.errors, ast,
        "can only get the digest of a field, local, parameter or this");
      return false;
  }

  // Set the type to U64.
  ast_t* type = type_builtin(opt, expr, "U64");
  ast_settype(ast, type);
  return true;
}

bool expr_this(pass_opt_t* opt, ast_t* ast)
{
  if(opt->check.frame->def_arg != NULL)
  {
    ast_error(opt->check.errors, ast,
      "can't reference 'this' in a default argument");
    return false;
  }

  if(ast_id(ast_child(opt->check.frame->method)) == TK_AT)
  {
    ast_error(opt->check.errors, ast,
      "can't reference 'this' in a bare method");
    return false;
  }

  token_id cap = cap_for_this(&opt->check);

  if(!cap_sendable(cap) && (opt->check.frame->recover != NULL))
  {
    ast_t* parent = ast_parent(ast);
    if((ast_id(parent) != TK_DOT) && (ast_id(parent) != TK_CHAIN))
      cap = TK_TAG;
  }

  bool make_arrow = false;

  if(cap == TK_BOX)
  {
    cap = TK_REF;
    make_arrow = true;
  }

  ast_t* type = type_for_this(opt, ast, cap, TK_NONE);

  if(make_arrow)
  {
    BUILD(arrow, ast, NODE(TK_ARROW, NODE(TK_THISTYPE) TREE(type)));
    type = arrow;
  }

  // Get the nominal type, which may be the right side of an arrow type.
  ast_t* nominal;
  bool arrow;

  if(ast_id(type) == TK_NOMINAL)
  {
    nominal = type;
    arrow = false;
  } else {
    nominal = ast_childidx(type, 1);
    arrow = true;
  }

  ast_t* typeargs = ast_childidx(nominal, 2);
  ast_t* typearg = ast_child(typeargs);

  while(typearg != NULL)
  {
    if(!expr_nominal(opt, &typearg))
    {
      ast_error(opt->check.errors, ast, "couldn't create a type for 'this'");
      ast_free(type);
      return false;
    }

    typearg = ast_sibling(typearg);
  }

  if(!expr_nominal(opt, &nominal))
  {
    ast_error(opt->check.errors, ast, "couldn't create a type for 'this'");
    ast_free(type);
    return false;
  }

  // Handle cases in constructors where the type is incomplete (not all fields
  // have been defined yet); in these cases, we consider `this` to be a tag,
  // But if we're using it for accessing a field, we can allow the normal refcap
  // because if the field is defined, we're allowed to read it and if it's
  // undefined we'll be allowed to write to it to finish defining it.
  // The AST_FLAG_INCOMPLETE flag is set during the refer pass.
  if(ast_checkflag(ast, AST_FLAG_INCOMPLETE))
  {
    bool incomplete_ok = false;

    // We consider it to be okay to be incomplete if on the left side of a dot,
    // where the dot points to a field reference.
    ast_t* parent = ast_parent(ast);
    if((ast_id(parent) == TK_DOT) && (ast_child(parent) == ast))
    {
      ast_t* def = (ast_t*)ast_data(parent);
      pony_assert(def != NULL);

      switch(ast_id(def))
      {
        case TK_FVAR:
        case TK_FLET:
        case TK_EMBED: incomplete_ok = true; break;
        default: {}
      }
    }

    // If it's not considered okay to be incomplete, set the refcap to TK_TAG.
    if(!incomplete_ok)
    {
      ast_t* tag_type = set_cap_and_ephemeral(nominal, TK_TAG, TK_NONE);
      ast_setflag(tag_type, AST_FLAG_INCOMPLETE);
      ast_replace(&nominal, tag_type);
    }
  }

  if(arrow)
    type = ast_parent(nominal);
  else
    type = nominal;

  ast_settype(ast, type);
  return true;
}

bool expr_tuple(pass_opt_t* opt, ast_t* ast)
{
  ast_t* child = ast_child(ast);
  ast_t* type;

  if(ast_sibling(child) == NULL)
  {
    type = ast_type(child);
  } else {
    type = ast_from(ast, TK_TUPLETYPE);

    while(child != NULL)
    {
      if(ast_checkflag(child, AST_FLAG_JUMPS_AWAY))
      {
        ast_error(opt->check.errors, child,
          "a tuple can't contain an expression that jumps away with no value");
        return false;
      }

      ast_t* c_type = ast_type(child);

      if((c_type == NULL) || (ast_id(c_type) == TK_ERRORTYPE))
        return false;

      if(is_type_literal(c_type))
      {
        // At least one tuple member is literal, so whole tuple is
        ast_free(type);
        make_literal_type(ast);
        return true;
      }

      ast_append(type, c_type);
      child = ast_sibling(child);
    }
  }

  ast_settype(ast, type);
  return true;
}

bool expr_nominal(pass_opt_t* opt, ast_t** astp)
{
  // Resolve type aliases and typeparam references.
  if(!names_nominal(opt, *astp, astp, true))
    return false;

  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_TYPEPARAMREF:
      return flatten_typeparamref(opt, ast) == AST_OK;

    case TK_NOMINAL:
      break;

    default:
      return true;
  }

  // If still nominal, check constraints.
  ast_t* def = (ast_t*)ast_data(ast);
  pony_assert(def != NULL);

  // Special case: don't check the constraint of a Pointer or an Array. These
  // builtin types have no contraint on their type parameter, and it is safe
  // to bind a struct as a type argument (which is not safe on any user defined
  // type, as that type might then be used for pattern matching).
  if(is_pointer(ast) || is_literal(ast, "Array"))
    return true;

  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* typeargs = ast_childidx(ast, 2);

  if(!reify_defaults(typeparams, typeargs, true, opt))
    return false;

  if(is_maybe(ast))
  {
    // MaybePointer[A] must be bound to a struct.
    pony_assert(ast_childcount(typeargs) == 1);
    ast_t* typeparam = ast_child(typeparams);
    ast_t* typearg = ast_child(typeargs);
    bool ok = false;

    switch(ast_id(typearg))
    {
      case TK_NOMINAL:
      {
        ast_t* def = (ast_t*)ast_data(typearg);
        pony_assert(def != NULL);

        ok = ast_id(def) == TK_STRUCT;
        break;
      }

      case TK_TYPEPARAMREF:
      {
        ast_t* def = (ast_t*)ast_data(typearg);
        pony_assert(def != NULL);

        ok = def == typeparam;
        break;
      }

      default: {}
    }

    if(!ok)
    {
      ast_error(opt->check.errors, ast,
        "%s is not allowed: "
        "the type argument to MaybePointer must be a struct",
        ast_print_type(ast));

      return false;
    }

    return true;
  }

  return check_constraints(typeargs, typeparams, typeargs, true, opt);
}

static bool check_return_type(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, type, can_error, body);

  // The last statement is an error, and we've already checked any return
  // expressions in the method.
  if(ast_checkflag(body, AST_FLAG_JUMPS_AWAY))
    return true;

  ast_t* body_type = ast_type(body);
  if(is_typecheck_error(body_type))
    return false;

  // If it's a compiler intrinsic, ignore it.
  if(ast_id(body_type) == TK_COMPILE_INTRINSIC)
    return true;

  // The body type must match the return type, without subsumption, or an alias
  // of the body type must be a subtype of the return type.
  ast_t* a_type = alias(type);
  ast_t* a_body_type = alias(body_type);
  bool ok = true;

  errorframe_t info = NULL;
  if(!is_subtype(body_type, type, &info, opt) ||
    !is_subtype(a_body_type, a_type, &info, opt))
  {
    errorframe_t frame = NULL;
    ast_t* last = ast_childlast(body);
    ast_error_frame(&frame, last, "function body isn't the result type");
    ast_error_frame(&frame, type, "function return type: %s",
      ast_print_type(type));
    ast_error_frame(&frame, body_type, "function body type: %s",
      ast_print_type(body_type));
    errorframe_append(&frame, &info);
    errorframe_report(&frame, opt->check.errors);
    ok = false;
  }

  ast_free_unattached(a_type);
  ast_free_unattached(a_body_type);
  return ok;
}

bool expr_fun(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, type, can_error, body);

  if(ast_id(body) == TK_NONE)
    return true;

  if(!coerce_literals(&body, type, opt))
    return false;

  switch(ast_id(ast))
  {
    case TK_NEW:
    {
      bool ok = true;

      if(is_machine_word(type))
      {
        if(!check_return_type(opt, ast))
         ok = false;
      }

      return ok;
    }

    case TK_FUN:
      return check_return_type(opt, ast);

    default: {}
  }

  return true;
}

bool expr_compile_intrinsic(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  ast_settype(ast, ast_from(ast, TK_COMPILE_INTRINSIC));
  return true;
}
