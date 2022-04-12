#include "lambda.h"
#include "literal.h"
#include "reference.h"
#include "../ast/astbuild.h"
#include "../ast/id.h"
#include "../ast/printbuf.h"
#include "../pass/expr.h"
#include "../pass/pass.h"
#include "../pass/refer.h"
#include "../pass/syntax.h"
#include "../type/alias.h"
#include "../type/assemble.h"
#include "../type/cap.h"
#include "../type/reify.h"
#include "../type/sanitise.h"
#include "../type/subtype.h"
#include "../pkg/package.h"
#include "ponyassert.h"


// Process the given capture and create the AST for the corresponding field.
// Returns the create field AST in out_field, which must be freed by the caller,
// or NULL if there is no field to create.
// Returns false on error.
static bool make_capture_field(pass_opt_t* opt, ast_t* capture,
  ast_t** out_field)
{
  pony_assert(capture != NULL);
  pony_assert(out_field != NULL);

  AST_GET_CHILDREN(capture, id_node, type, value);
  const char* name = ast_name(id_node);

  bool is_dontcare = is_name_dontcare(name);

  // There are 3 varieties of capture:
  // x -> capture variable x, type from defn of x
  // x = y -> capture expression y, type inferred from expression type
  // x: T = y -> capture expression y, type T

  if(ast_id(value) == TK_NONE)
  {
    // Variable capture
    pony_assert(ast_id(type) == TK_NONE);

    if(is_dontcare)
    {
      *out_field = NULL;
      return true;
    }

    ast_t* def = ast_get(capture, name, NULL);

    if(def == NULL)
    {
      ast_error(opt->check.errors, id_node,
        "cannot capture \"%s\", variable not defined", name);
      return false;
    }

    // lambda captures used before their declaration with their type
    // not defined are not legal
    if(!def_before_use(opt, def, capture, name))
      return false;

    switch(ast_id(def))
    {
      case TK_VAR:
      case TK_LET:
      case TK_PARAM:
      case TK_MATCH_CAPTURE:
      case TK_FVAR:
      case TK_FLET:
      case TK_EMBED:
        break;

      default:
        ast_error(opt->check.errors, id_node, "cannot capture \"%s\", can only "
          "capture fields, parameters and local variables", name);
        return false;
    }

    BUILD(capture_rhs, id_node, NODE(TK_REFERENCE, ID(name)));

    type = alias(ast_type(def));
    value = capture_rhs;
  } else if(ast_id(type) == TK_NONE) {
    // No type specified, use type of the captured expression
    if(ast_type(value) == NULL)
      return false;

    type = alias(ast_type(value));
  } else {
    // Type given, infer literals
    if(!coerce_literals(&value, type, opt))
      return false;

    // Check subtyping now if we're capturing into '_', since the field will be
    // discarded.
    if(is_dontcare)
    {
      ast_t* p_type = consume_type(type, TK_NONE);
      ast_t* v_type = ast_type(value);
      errorframe_t info = NULL;
      errorframe_t frame = NULL;

      if(p_type == NULL)
      {
        ast_error_frame(&frame, type,
          "invalid parameter type: %s",
          ast_print_type(type));
        errorframe_append(&frame, &info);
        errorframe_report(&frame, opt->check.errors);
        ast_free_unattached(p_type);
        return false;
      }
      else if(!is_subtype(v_type, p_type, &info, opt))
      {
        ast_error_frame(&frame, value, "argument not assignable to parameter");
        ast_error_frame(&frame, value, "argument type is %s",
                        ast_print_type(v_type));
        ast_error_frame(&frame, id_node, "parameter type requires %s",
                        ast_print_type(p_type));
        errorframe_append(&frame, &info);
        errorframe_report(&frame, opt->check.errors);
        ast_free_unattached(p_type);
        return false;
      }

      ast_free_unattached(p_type);
    }
  }

  if(is_typecheck_error(type))
    return false;

  if(is_dontcare)
  {
    *out_field = NULL;
    return true;
  }

  type = sanitise_type(type);

  BUILD(field, id_node,
    NODE(TK_FVAR,
      TREE(id_node)
      TREE(type)
      TREE(value)
      NONE));

  *out_field = field;
  return true;
}


static void find_possible_fun_defs(pass_opt_t* opt, ast_t* ast,
  astlist_t** fun_defs, astlist_t** obj_caps)
{
  switch(ast_id(ast))
  {
    case TK_NOMINAL:
    {
      // A lambda type definition must be an interface.
      ast_t* def = (ast_t*)ast_data(ast);
      if(ast_id(def) != TK_INTERFACE)
        return;

      // The interface must specify just one method in its members.
      ast_t* members = ast_childidx(def, 4);
      pony_assert(ast_id(members) == TK_MEMBERS);
      if(ast_childcount(members) != 1)
        return;

      // That one method is the fun def that we're looking for.
      ast_t* fun_def = ast_child(members);

      // If the interface type has type parameters, we need to reify.
      ast_t* typeargs = ast_childidx(ast, 2);
      ast_t* typeparams = ast_childidx(def, 1);
      if((ast_id(typeargs) == TK_TYPEARGS) &&
        (ast_id(typeparams) == TK_TYPEPARAMS)
        )
        fun_def = reify_method_def(fun_def, typeparams, typeargs, opt);

      // Return the object cap and the method definition.
      *obj_caps = astlist_push(*obj_caps, ast_childidx(ast, 3));
      *fun_defs = astlist_push(*fun_defs, fun_def);
      break;
    }

    case TK_ARROW:
      find_possible_fun_defs(opt, ast_childidx(ast, 1), fun_defs, obj_caps);
      break;

    case TK_TYPEPARAMREF:
    {
      ast_t* def = (ast_t*)ast_data(ast);
      pony_assert(ast_id(def) == TK_TYPEPARAM);
      find_possible_fun_defs(opt, ast_childidx(def, 1), fun_defs, obj_caps);
      break;
    }

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      for(ast_t* c = ast_child(ast); c != NULL; c = ast_sibling(c))
        find_possible_fun_defs(opt, c, fun_defs, obj_caps);
      break;
    }

    default:
      break;
  }
}


bool expr_lambda(pass_opt_t* opt, ast_t** astp)
{
  pony_assert(astp != NULL);
  ast_t* ast = *astp;
  pony_assert(ast != NULL);

  AST_GET_CHILDREN(ast, receiver_cap, name, t_params, params, captures,
    ret_type, raises, body, obj_cap);
  ast_t* annotation = ast_consumeannotation(ast);

  // Try to find an antecedent type, and find possible lambda interfaces in it.
  ast_t* antecedent_type = find_antecedent_type(opt, ast, NULL);
  astlist_t* possible_fun_defs = NULL;
  astlist_t* possible_obj_caps = NULL;
  if(!is_typecheck_error(antecedent_type))
    find_possible_fun_defs(opt, antecedent_type, &possible_fun_defs,
      &possible_obj_caps);

  // If there's more than one possible fun defs, rule out impossible ones by
  // comparing each fun def by some basic criteria against the lambda,
  // creating a new list containing only the remaining possibilities.
  if(astlist_length(possible_fun_defs) > 1)
  {
    astlist_t* new_fun_defs = NULL;
    astlist_t* new_obj_caps = NULL;

    astlist_t* fun_def_cursor = possible_fun_defs;
    astlist_t* obj_cap_cursor = possible_obj_caps;
    for(; (fun_def_cursor != NULL) && (obj_cap_cursor != NULL);
      fun_def_cursor = astlist_next(fun_def_cursor),
      obj_cap_cursor = astlist_next(obj_cap_cursor))
    {
      ast_t* fun_def = astlist_data(fun_def_cursor);
      ast_t* def_obj_cap = astlist_data(obj_cap_cursor);

      if(is_typecheck_error(fun_def))
        continue;

      AST_GET_CHILDREN(fun_def, def_receiver_cap, def_name, def_t_params,
        def_params, def_ret_type, def_raises);

      // Must have the same number of parameters.
      if(ast_childcount(params) != ast_childcount(def_params))
        continue;

      // Must have a supercap of the def's receiver cap (if present).
      if((ast_id(receiver_cap) != TK_NONE) && (ast_id(receiver_cap) != TK_AT) &&
        !is_cap_sub_cap(ast_id(def_receiver_cap), TK_NONE,
        ast_id(receiver_cap), TK_NONE)
        )
        continue;

      // Must have a supercap of the def's object cap (if present).
      if((ast_id(obj_cap) != TK_NONE) &&
        !is_cap_sub_cap(ast_id(obj_cap), TK_NONE, ast_id(def_obj_cap), TK_NONE))
        continue;

      // TODO: This logic could potentially be expanded to do deeper
      // compatibility checks, but checks involving subtyping here would be
      // difficult, because the lambda's AST is not caught up yet in the passes.

      new_fun_defs = astlist_push(new_fun_defs, fun_def);
      new_obj_caps = astlist_push(new_obj_caps, def_obj_cap);
    }

    astlist_free(possible_fun_defs);
    astlist_free(possible_obj_caps);
    possible_fun_defs = new_fun_defs;
    possible_obj_caps = new_obj_caps;
  }

  if(astlist_length(possible_fun_defs) == 1)
  {
    ast_t* fun_def = astlist_data(possible_fun_defs);
    ast_t* def_obj_cap = astlist_data(possible_obj_caps);

    // Try to complete the lambda's type info by inferring from the lambda type.
    if(!is_typecheck_error(fun_def))
    {
      // Infer the object cap, receiver cap, and return type if unspecified.
      if(ast_id(obj_cap) == TK_NONE)
        ast_replace(&obj_cap, def_obj_cap);
      if(ast_id(receiver_cap) == TK_NONE)
        ast_replace(&receiver_cap, ast_child(fun_def));
      if(ast_id(ret_type) == TK_NONE)
        ast_replace(&ret_type, ast_childidx(fun_def, 4));

      // Infer the type of any parameters that were left unspecified.
      ast_t* param = ast_child(params);
      ast_t* def_param = ast_child(ast_childidx(fun_def, 3));
      while((param != NULL) && (def_param != NULL))
      {
        ast_t* param_id = ast_child(param);
        ast_t* param_type = ast_sibling(param_id);

        // Convert a "_" parameter to whatever the expected parameter is.
        if(is_name_dontcare(ast_name(param_id)))
        {
          ast_replace(&param_id, ast_child(def_param));
          ast_replace(&param_type, ast_childidx(def_param, 1));
        }
        // Give a type-unspecified parameter the type of the expected parameter.
        else if(ast_id(param_type) == TK_NONE)
        {
          ast_replace(&param_type, ast_childidx(def_param, 1));
        }

        param = ast_sibling(param);
        def_param = ast_sibling(def_param);
      }
    }

    ast_free_unattached(fun_def);
  }

  astlist_free(possible_obj_caps);

  // If any parameters still have no type specified, it's an error.
  ast_t* param = ast_child(params);
  while(param != NULL)
  {
    if(ast_id(ast_childidx(param, 1)) == TK_NONE)
    {
      ast_error(opt->check.errors, param,
        "a lambda parameter must specify a type or be inferable from context");

      if(astlist_length(possible_fun_defs) > 1)
      {
        for(astlist_t* fun_def_cursor = possible_fun_defs;
          fun_def_cursor != NULL;
          fun_def_cursor = astlist_next(fun_def_cursor))
        {
          ast_error_continue(opt->check.errors, astlist_data(fun_def_cursor),
            "this lambda interface is inferred, but it's not the only one");
        }
      }

      astlist_free(possible_fun_defs);
      return false;
    }
    param = ast_sibling(param);
  }

  astlist_free(possible_fun_defs);

  bool bare = ast_id(ast) == TK_BARELAMBDA;
  ast_t* members = ast_from(ast, TK_MEMBERS);
  ast_t* last_member = NULL;
  bool failed = false;

  if(bare)
    pony_assert(ast_id(captures) == TK_NONE);

  // Process captures
  for(ast_t* p = ast_child(captures); p != NULL; p = ast_sibling(p))
  {
    ast_t* field = NULL;
    bool ok = make_capture_field(opt, p, &field);

    if(field != NULL)
      ast_list_append(members, &last_member, field);
    else if(!ok)
      // An error occurred, just keep going to potentially find more errors
      failed = true;
  }

  if(failed)
  {
    ast_free(members);
    return false;
  }

  // Stop the various elements being marked as preserve
  ast_clearflag(t_params, AST_FLAG_PRESERVE);
  ast_clearflag(params, AST_FLAG_PRESERVE);
  ast_clearflag(ret_type, AST_FLAG_PRESERVE);
  ast_clearflag(body, AST_FLAG_PRESERVE);

  const char* fn_name = "apply";

  if(ast_id(name) == TK_ID)
    fn_name = ast_name(name);

  // Make the apply function
  BUILD(apply, ast,
    NODE(TK_FUN, AST_SCOPE
      ANNOTATE(annotation)
      TREE(receiver_cap)
      ID(fn_name)
      TREE(t_params)
      TREE(params)
      TREE(ret_type)
      TREE(raises)
      TREE(body)
      NONE)); // Doc string

  ast_list_append(members, &last_member, apply);
  ast_setflag(members, AST_FLAG_PRESERVE);

  printbuf_t* buf = printbuf_new();
  printbuf(buf, bare ? "@{(" : "{(");
  bool first = true;

  for(ast_t* p = ast_child(params); p != NULL; p = ast_sibling(p))
  {
    if(first)
      first = false;
    else
      printbuf(buf, ", ");

    printbuf(buf, "%s", ast_print_type(ast_childidx(p, 1)));
  }

  printbuf(buf, ")");

  if(ast_id(ret_type) != TK_NONE)
    printbuf(buf, ": %s", ast_print_type(ret_type));

  if(ast_id(raises) != TK_NONE)
    printbuf(buf, " ?");

  printbuf(buf, "}");

  // Replace lambda with object literal
  REPLACE(astp,
    NODE(TK_OBJECT, DATA(stringtab(buf->m))
      TREE(obj_cap)
      NONE  // Provides list
      TREE(members)));

  printbuf_free(buf);

  if(bare)
  {
    BUILD(bare_annotation, *astp,
      NODE(TK_ANNOTATION,
        ID("ponyint_bare")));

    // Record the syntax pass as done to avoid the error about internal
    // annotations.
    ast_pass_record(bare_annotation, PASS_SYNTAX);
    ast_setannotation(*astp, bare_annotation);
  }

  // Catch up passes
  if(ast_visit(astp, pass_syntax, NULL, opt, PASS_SYNTAX) != AST_OK)
    return false;

  return ast_passes_subtree(astp, opt, PASS_EXPR);
}


static bool capture_from_reference(pass_opt_t* opt, ast_t* ctx, ast_t* ast,
  ast_t* captures, ast_t** last_capture)
{
  const char* name = ast_name(ast_child(ast));

  if(is_name_dontcare(name))
    return true;

  ast_t* refdef = ast_get(ast, name, NULL);

  if(refdef != NULL)
    return true;

  refdef = ast_get(ctx, name, NULL);

  if(refdef == NULL)
  {
    ast_error(opt->check.errors, ast,
      "cannot capture \"%s\", variable not defined", name);
    return false;
  }

  if(!def_before_use(opt, refdef, ctx, name))
    return false;

  switch(ast_id(refdef))
  {
    case TK_VAR:
    case TK_LET:
    case TK_PARAM:
    case TK_MATCH_CAPTURE:
    case TK_FVAR:
    case TK_FLET:
    case TK_EMBED:
      break;

    default:
      ast_error(opt->check.errors, ast, "cannot capture \"%s\", can only "
        "capture fields, parameters and local variables", name);
      return NULL;
  }

  // Check if we've already captured it
  for(ast_t* p = ast_child(captures); p != NULL; p = ast_sibling(p))
  {
    AST_GET_CHILDREN(p, c_name, c_type);

    if(name == ast_name(c_name))
      return true;
  }

  ast_t* type = alias(ast_type(refdef));

  if(is_typecheck_error(type))
    return false;

  type = sanitise_type(type);

  BUILD(field, ast,
    NODE(TK_FVAR,
      ID(name)
      TREE(type)
      NODE(TK_REFERENCE, ID(name))
      NONE));

  ast_list_append(captures, last_capture, field);
  return true;
}


static bool capture_from_expr(pass_opt_t* opt, ast_t* ctx, ast_t* ast,
  ast_t* capture, ast_t** last_capture)
{
  // Skip preserved ASTs.
  if(ast_checkflag(ast, AST_FLAG_PRESERVE))
    return true;

  bool ok = true;

  if(ast_id(ast) == TK_REFERENCE)
  {
    // Try to capture references.
    if(!capture_from_reference(opt, ctx, ast, capture, last_capture))
      ok = false;
  } else {
    // Proceed down through all child ASTs.
    for(ast_t* p = ast_child(ast); p != NULL; p = ast_sibling(p))
    {
      if(!capture_from_expr(opt, ctx, p, capture, last_capture))
        ok = false;
    }
  }

  return ok;
}


static bool capture_from_type(pass_opt_t* opt, ast_t* ctx, ast_t** def,
  ast_t* capture, ast_t** last_capture)
{
  // Turn any free variables into fields.
  if(!ast_passes_type(def, opt, PASS_SCOPE))
    return false;

  bool ok = true;
  ast_t* members = ast_childidx(*def, 4);

  for(ast_t* p = ast_child(members); p != NULL; p = ast_sibling(p))
  {
    switch(ast_id(p))
    {
      case TK_FUN:
      case TK_BE:
      {
        if(ast_id(ast_child(p)) != TK_AT)
        {
          ast_t* body = ast_childidx(p, 6);

          if(!capture_from_expr(opt, ctx, body, capture, last_capture))
            ok = false;
        }

        break;
      }

      default: {}
    }
  }

  // Reset the scope.
  ast_clear(*def);
  return ok;
}


static void add_field_to_object(pass_opt_t* opt, ast_t* field,
  ast_t* class_members, ast_t* create_params, ast_t* create_body,
  ast_t* call_args)
{
  AST_GET_CHILDREN(field, id, type, init);
  ast_t* p_id = ast_from_string(id, package_hygienic_id(&opt->check));

  // The param is: $0: type
  BUILD(param, field,
    NODE(TK_PARAM,
      TREE(p_id)
      TREE(type)
      NONE));

  // The arg is: $seq init
  BUILD(arg, init,
    NODE(TK_SEQ,
      TREE(init)));

  // The body of create contains: id = consume $0
  BUILD(assign, init,
    NODE(TK_ASSIGN,
      NODE(TK_REFERENCE, TREE(id))
      NODE(TK_CONSUME, NODE(TK_NONE) NODE(TK_REFERENCE, TREE(p_id)))));

  // Remove the initialiser from the field
  ast_replace(&init, ast_from(init, TK_NONE));

  ast_add(class_members, field);
  ast_append(create_params, param);
  ast_append(create_body, assign);
  ast_append(call_args, arg);
}


static bool catch_up_provides(pass_opt_t* opt, ast_t* provides)
{
  // Make sure all traits have been through the expr pass before proceeding.
  // We run into dangling ast_data pointer problems when we inherit methods from
  // traits that have only been through the refer pass, and not the expr pass.

  ast_t* child = ast_child(provides);

  while(child != NULL)
  {
    if(!ast_passes_subtree(&child, opt, PASS_EXPR))
      return false;

    ast_t* def = (ast_t*)ast_data(child);

    if(def != NULL && !ast_passes_type(&def, opt, PASS_EXPR))
      return false;

    child = ast_sibling(child);
  }

  return true;
}


bool expr_object(pass_opt_t* opt, ast_t** astp)
{
  ast_t* ast = *astp;
  bool ok = true;

  AST_GET_CHILDREN(ast, cap, provides, members);
  ast_clearflag(cap, AST_FLAG_PRESERVE);
  ast_clearflag(provides, AST_FLAG_PRESERVE);
  ast_clearflag(members, AST_FLAG_PRESERVE);

  ast_t* annotation = ast_consumeannotation(ast);
  const char* c_id = package_hygienic_id(&opt->check);

  ast_t* t_params;
  ast_t* t_args;
  collect_type_params(ast, &t_params, &t_args);

  const char* nice_id = (const char*)ast_data(ast);

  if(nice_id == NULL)
    nice_id = "object literal";

  // Create a new anonymous type.
  BUILD(def, ast,
    NODE(TK_CLASS, AST_SCOPE
      ANNOTATE(annotation)
      NICE_ID(c_id, nice_id)
      TREE(t_params)
      NONE
      TREE(provides)
      NODE(TK_MEMBERS)
      NONE
      NONE));

  // We will have a create method in the type.
  BUILD(create, members,
    NODE(TK_NEW, AST_SCOPE
      NONE
      ID("create")
      NONE
      NODE(TK_PARAMS)
      NONE
      NONE
      NODE(TK_SEQ,
        NODE(TK_TRUE))
      NONE));

  BUILD(type_ref, ast, NODE(TK_REFERENCE, ID(c_id)));

  if(ast_id(t_args) != TK_NONE)
  {
    // Need to add type args to our type reference
    BUILD(t, ast, NODE(TK_QUALIFY, TREE(type_ref) TREE(t_args)));
    type_ref = t;
  }

  ast_free_unattached(t_args);

  // We will replace object..end with $0.create(...)
  BUILD(call, ast,
    NODE(TK_CALL,
      NODE(TK_DOT,
        TREE(type_ref)
        ID("create"))
      NODE(TK_POSITIONALARGS)
      NONE
      NONE));

  ast_t* create_params = ast_childidx(create, 3);
  ast_t* create_body = ast_childidx(create, 6);
  ast_t* call_args = ast_childidx(call, 1);
  ast_t* class_members = ast_childidx(def, 4);
  ast_t* member = ast_child(members);

  bool has_fields = false;
  bool has_behaviours = false;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      case TK_EMBED:
      {
        add_field_to_object(opt, member, class_members, create_params,
          create_body, call_args);

        has_fields = true;
        break;
      }

      case TK_BE:
        // If we have behaviours, we must be an actor.
        ast_append(class_members, member);
        has_behaviours = true;
        break;

      default:
        // Keep all the methods as they are.
        ast_append(class_members, member);
        break;
    }

    member = ast_sibling(member);
  }

  // Add the create function at the end.
  ast_append(class_members, create);

  // Add new type to current module and bring it up to date with passes.
  ast_t* module = ast_nearest(ast, TK_MODULE);
  ast_append(module, def);

  // Turn any free variables into fields.
  ast_t* captures = ast_from(ast, TK_MEMBERS);
  ast_t* last_capture = NULL;

  if(!capture_from_type(opt, *astp, &def, captures, &last_capture))
    ok = false;

  for(ast_t* p = ast_child(captures); p != NULL; p = ast_sibling(p))
  {
    add_field_to_object(opt, p, class_members, create_params, create_body,
      call_args);
    has_fields = true;
  }

  ast_free_unattached(captures);
  ast_resetpass(def, PASS_SUGAR);

  // Handle capability and whether the anonymous type is a class, primitive or
  // actor.
  token_id cap_id = ast_id(cap);

  if(has_behaviours)
  {
    // Change the type to an actor.
    ast_setid(def, TK_ACTOR);

    if(cap_id != TK_NONE && cap_id != TK_TAG)
    {
      ast_error(opt->check.errors, cap, "object literals with behaviours are "
        "actors and so must have tag capability");
      ok = false;
    }

    cap_id = TK_TAG;
  }
  else if(!has_fields && (cap_id == TK_NONE || cap_id == TK_TAG ||
    cap_id == TK_BOX || cap_id == TK_VAL))
  {
    // Change the type from a class to a primitive.
    ast_setid(def, TK_PRIMITIVE);
    cap_id = TK_VAL;
  }

  if(ast_id(def) != TK_PRIMITIVE)
    pony_assert(!ast_has_annotation(def, "ponyint_bare"));

  // Reset constructor to pick up the correct defaults.
  ast_setid(ast_child(create), cap_id);
  ast_t* result = ast_childidx(create, 4);
  ast_replace(&result,
    type_for_class(opt, def, result, cap_id, TK_EPHEMERAL, false));

  // Catch up provides before catching up the entire type.
  if(!catch_up_provides(opt, provides))
    return false;

  // Type check the anonymous type.
  if(!ast_passes_type(&def, opt, PASS_EXPR))
    return false;

  // Replace object..end with $0.create(...)
  ast_replace(astp, call);

  if(ast_visit(astp, pass_syntax, NULL, opt, PASS_SYNTAX) != AST_OK)
    return false;

  if(!ast_passes_subtree(astp, opt, PASS_EXPR))
    return false;

  return ok;
}
