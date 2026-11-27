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
#include "../type/lookup.h"
#include "../type/sanitise.h"
#include "../pass/traits.h"
#include "../pkg/program.h"
#include "../type/subtype.h"
#include "../type/typealias.h"
#include "../type/typeparam.h"
#include "../type/viewpoint.h"
#include "../pkg/package.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <string.h>

// The internal annotation on the object literal a lambda becomes when all the
// lambda's types are written.
#define TYPED_LAMBDA_ANNOTATION "ponyint_typed_lambda"

// The internal annotation on an object literal a lambda became.
#define LAMBDA_OBJECT_ANNOTATION "ponyint_lambda_object"


// For a field definition, return the aliased type as seen through `this` in
// the current method (viewpoint-adapted). For a local/parameter, return the
// aliased type directly. When inside an iftype body, embeds the narrowed
// constraint as an explicit intersection so it survives sanitise_type.
static ast_t* capture_type(pass_opt_t* opt, ast_t* capture, ast_t* def)
{
  switch(ast_id(def))
  {
    case TK_FVAR:
    case TK_FLET:
    case TK_EMBED:
    {
      token_id cap = cap_for_this(&opt->check);
      ast_t* f_type = ast_type(def);
      f_type = typeparam_embed_narrowing(opt, f_type, capture);
      BUILD(cap_ast, f_type, NODE(cap));
      ast_t* adapted = viewpoint_type(cap_ast, f_type, opt);
      ast_free_unattached(cap_ast);

      if(adapted == NULL)
        return NULL;

      if(ast_id(adapted) == TK_ARROW)
      {
        ast_t* upper = viewpoint_upper(adapted, opt);

        if(upper == NULL)
        {
          ast_t* this_type = type_for_this(opt, capture, cap, TK_NONE);
          ast_error(opt->check.errors, capture,
            "can't read a field through %s",
            ast_print_type(this_type, opt->strtab));
          ast_free_unattached(this_type);
          ast_free_unattached(adapted);
          return NULL;
        }

        ast_free_unattached(upper);
      }

      ast_t* result = alias(adapted, opt);

      if(result != adapted)
        ast_free_unattached(adapted);

      return result;
    }

    default:
    {
      ast_t* type = ast_type(def);
      type = typeparam_embed_narrowing(opt, type, capture);
      return alias(type, opt);
    }
  }
}


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

  // The two `= y` forms capture an arbitrary expression, which can't jump away
  // with no value. (The bare `x` form has a TK_NONE value, which the helper
  // ignores.)
  if(jumps_away_no_value(opt, value, "a lambda capture"))
    return false;

  if(ast_id(value) == TK_NONE)
  {
    // Variable capture
    pony_assert(ast_id(type) == TK_NONE);

    if(is_dontcare)
    {
      *out_field = NULL;
      return true;
    }

    sym_status_t status;
    ast_t* def = ast_get(capture, name, &status);

    if(def == NULL)
    {
      ast_error(opt->check.errors, id_node,
        "cannot capture \"%s\", variable not defined", name);
      return false;
    }

    // lambda captures used before their declaration with their type
    // not defined are not legal
    //
    // After trait body adoption, the parameter definition and the body
    // reference can originate from different traits, so their line numbers
    // are not comparable.
    if(ast_id(def) != TK_PARAM ||
      ((opt->check.frame->method != NULL) &&
       (ast_data(opt->check.frame->method) == opt->check.frame->type)))
    {
      if(!def_before_use(opt, def, capture, name))
        return false;
    }

    switch(ast_id(def))
    {
      case TK_VAR:
      case TK_LET:
      case TK_PARAM:
      case TK_MATCH_CAPTURE:
        break;

      case TK_FVAR:
      case TK_FLET:
      case TK_EMBED:
        if(status == SYM_UNDEFINED)
        {
          ast_error(opt->check.errors, id_node,
            "can't capture field \"%s\" before it's initialized", name);
          return false;
        }
        break;

      default:
        ast_error(opt->check.errors, id_node, "cannot capture \"%s\", can only "
          "capture fields, parameters and local variables", name);
        return false;
    }

    BUILD(capture_rhs, id_node, NODE(TK_REFERENCE, ID(name)));

    type = capture_type(opt, capture, def);
    value = capture_rhs;
  } else if(ast_id(type) == TK_NONE) {
    // No type specified, use type of the captured expression
    if(ast_type(value) == NULL)
      return false;

    type = alias(ast_type(value), opt);
  } else {
    // Type given, infer literals
    if(!coerce_literals(&value, type, opt))
      return false;

    // Check subtyping now if we're capturing into '_', since the field will be
    // discarded.
    if(is_dontcare)
    {
      ast_t* p_type = consume_type(type, TK_NONE, false, opt);
      ast_t* v_type = ast_type(value);
      errorframe_t info = NULL;
      errorframe_t frame = NULL;

      if(p_type == NULL)
      {
        ast_error_frame(&frame, type,
          "invalid parameter type: %s",
          ast_print_type(type, opt->strtab));
        errorframe_append(&frame, &info);
        errorframe_report(&frame, opt->check.errors);
        ast_free_unattached(p_type);
        return false;
      }
      else if(!is_subtype(v_type, p_type, &info, opt))
      {
        ast_error_frame(&frame, value, "argument not assignable to parameter");
        ast_error_frame(&frame, value, "argument type is %s",
                        ast_print_type(v_type, opt->strtab));
        ast_error_frame(&frame, id_node, "parameter type requires %s",
                        ast_print_type(p_type, opt->strtab));
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

  type = sanitise_type(type, opt);

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

      if(ast_id(fun_def) == TK_METHODGROUP)
        fun_def = ast_child(fun_def);

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

    case TK_TYPEALIASREF:
    {
      ast_t* unfolded = typealias_unfold(ast);

      if(unfolded != NULL)
      {
        // Recurse into temporary local lists. The recursion may store
        // pointers into the unfolded tree in obj_caps (the obj_cap at
        // child 3 of any TK_NOMINAL found inside the unfolded result).
        // We dup those pointers before freeing the unfolded tree so the
        // caller doesn't hold dangling references. fun_defs entries
        // point to permanent type definitions (via ast_data) or are
        // freshly allocated by reify_method_def, so they don't need
        // duping.
        astlist_t* local_fun_defs = NULL;
        astlist_t* local_obj_caps = NULL;
        find_possible_fun_defs(opt, unfolded, &local_fun_defs,
          &local_obj_caps);

        astlist_t* fd = local_fun_defs;
        astlist_t* oc = local_obj_caps;
        while(fd != NULL && oc != NULL)
        {
          *fun_defs = astlist_push(*fun_defs, astlist_data(fd));

          // ast_dup copies the scope pointer from the original's parent
          // (which is inside the unfolded tree we're about to free).
          // Clear it to avoid a dangling scope reference.
          ast_t* dup_cap = ast_dup(astlist_data(oc));
          ast_set_scope(dup_cap, NULL);
          *obj_caps = astlist_push(*obj_caps, dup_cap);

          fd = astlist_next(fd);
          oc = astlist_next(oc);
        }

        astlist_free(local_fun_defs);
        astlist_free(local_obj_caps);
        ast_free_unattached(unfolded);
      }

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

  // Whether every parameter type and the result type are written, before
  // inference fills any in from the lambda's context.
  bool written_types = (ast_id(ret_type) != TK_NONE);
  for(ast_t* p = ast_child(params); written_types && (p != NULL);
    p = ast_sibling(p))
  {
    if(ast_id(ast_childidx(p, 1)) == TK_NONE)
      written_types = false;
  }

  ast_t* annotation = ast_consumeannotation(ast);

  // Try to find an antecedent type, and find possible lambda interfaces in it.
  ast_t* antecedent_owner = NULL;
  ast_t* antecedent_type = find_antecedent_type(opt, ast, NULL,
    &antecedent_owner);
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
      {
        ast_free_unattached(def_obj_cap);
        continue;
      }

      AST_GET_CHILDREN(fun_def, def_receiver_cap, def_name, def_t_params,
        def_params, def_ret_type, def_guard, def_raises);

      (void)def_guard;

      // Must have the same number of parameters.
      if(ast_childcount(params) != ast_childcount(def_params))
      {
        ast_free_unattached(def_obj_cap);
        continue;
      }

      // Must have a supercap of the def's receiver cap (if present).
      if((ast_id(receiver_cap) != TK_NONE) && (ast_id(receiver_cap) != TK_AT) &&
        !is_cap_sub_cap(ast_id(def_receiver_cap), TK_NONE,
        ast_id(receiver_cap), TK_NONE)
        )
      {
        ast_free_unattached(def_obj_cap);
        continue;
      }

      // Must have a supercap of the def's object cap (if present).
      if((ast_id(obj_cap) != TK_NONE) &&
        !is_cap_sub_cap(ast_id(obj_cap), TK_NONE, ast_id(def_obj_cap), TK_NONE))
      {
        ast_free_unattached(def_obj_cap);
        continue;
      }

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

  // Free any owned obj_cap entries remaining in the list. For caps from
  // the TK_TYPEALIASREF path these are owned dups; consumed caps (moved
  // into the tree by ast_replace) have a parent, so ast_free_unattached
  // is a no-op. For caps from permanent AST nominals they also have a
  // parent. This handles all list sizes uniformly: the == 1 case (where
  // ast_replace may or may not have consumed the cap), the > 1 case
  // (surviving candidates after filtering), and the 0 case (no-op).
  for(astlist_t* oc = possible_obj_caps; oc != NULL;
    oc = astlist_next(oc))
  {
    ast_free_unattached(astlist_data(oc));
  }
  astlist_free(possible_obj_caps);

  if(antecedent_owner != NULL)
    ast_free_unattached(antecedent_owner);

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

  // The body's nodes can refer to each other, such as a reference to a
  // narrowing that an iftype in the body declares. Copy it so those refer to
  // the copy's nodes; the lambda and its body are freed below.
  ast_t* body_copy = ast_dup_closed(body);

  // Make the apply function
  BUILD(apply, ast,
    NODE(TK_FUN, AST_SCOPE
      ANNOTATE(annotation)
      TREE(receiver_cap)
      ID(fn_name)
      TREE(t_params)
      TREE(params)
      TREE(ret_type)
      NONE
      TREE(raises)
      TREE(body_copy)
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

    printbuf(buf, "%s", ast_print_type(ast_childidx(p, 1), opt->strtab));
  }

  printbuf(buf, ")");

  if(ast_id(ret_type) != TK_NONE)
    printbuf(buf, ": %s", ast_print_type(ret_type, opt->strtab));

  if(ast_id(raises) != TK_NONE)
    printbuf(buf, " ?");

  printbuf(buf, "}");

  // Replace lambda with object literal
  REPLACE(astp,
    NODE(TK_OBJECT, DATA(stringtab(opt->strtab, buf->m))
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

  // A lambda with all its types written takes no types from its context.
  if(written_types)
    ast_add_internal_annotation(*astp, TYPED_LAMBDA_ANNOTATION, opt->strtab);

  ast_add_internal_annotation(*astp, LAMBDA_OBJECT_ANNOTATION, opt->strtab);

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

  sym_status_t status;
  refdef = ast_get(ctx, name, &status);

  if(refdef == NULL)
  {
    ast_error(opt->check.errors, ast,
      "cannot capture \"%s\", variable not defined", name);
    return false;
  }

  // After trait body adoption, the parameter definition and the body
  // reference can originate from different traits, so their line numbers
  // are not comparable.
  if(ast_id(refdef) != TK_PARAM ||
    ((opt->check.frame->method != NULL) &&
     (ast_data(opt->check.frame->method) == opt->check.frame->type)))
  {
    if(!def_before_use(opt, refdef, ctx, name))
      return false;
  }

  switch(ast_id(refdef))
  {
    case TK_VAR:
    case TK_LET:
    case TK_PARAM:
    case TK_MATCH_CAPTURE:
      break;

    case TK_FVAR:
    case TK_FLET:
    case TK_EMBED:
      if(status == SYM_UNDEFINED)
      {
        ast_error(opt->check.errors, ast,
          "can't capture field \"%s\" before it's initialized", name);
        return false;
      }
      break;

    default:
      ast_error(opt->check.errors, ast, "cannot capture \"%s\", can only "
        "capture fields, parameters and local variables", name);
      return false;
  }

  // Check if we've already captured it
  for(ast_t* p = ast_child(captures); p != NULL; p = ast_sibling(p))
  {
    AST_GET_CHILDREN(p, c_name, c_type);

    if(name == ast_name(c_name))
      return true;
  }

  ast_t* type = capture_type(opt, ast, refdef);

  if(is_typecheck_error(type))
    return false;

  type = sanitise_type(type, opt);

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
          ast_t* body = ast_childidx(p, 7);

          if(!capture_from_expr(opt, ctx, body, capture, last_capture))
            ok = false;
        }

        break;
      }

      case TK_METHODGROUP:
      {
        ast_t* mg_child = ast_child(p);
        while(mg_child != NULL)
        {
          if(ast_id(ast_child(mg_child)) != TK_AT)
          {
            ast_t* body = ast_childidx(mg_child, 7);

            if(!capture_from_expr(opt, ctx, body, capture, last_capture))
              ok = false;
          }
          mg_child = ast_sibling(mg_child);
        }
        break;
      }

      default: {}
    }
  }

  // Reset the scope. This empties every symtab in the subtree, so the scope
  // pass re-runs over it during the catch up. Passes that establish scope
  // bindings must therefore be idempotent on re-entry and must re-establish
  // those bindings here — see scope_iftype, which re-installs the narrowed
  // type parameters its first run parked in typeparam_store.
  ast_clear(*def);
  return ok;
}


static void add_field_to_object(pass_opt_t* opt, ast_t* field,
  ast_t* class_members, ast_t* create_params, ast_t* create_body,
  ast_t* call_args)
{
  AST_GET_CHILDREN(field, id, type, init);
  ast_t* p_id = ast_from_string(id, module_hygienic_id(&opt->check, opt), opt->strtab);

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


static ast_t* provided_def(ast_t* type, ast_t** unfolded)
{
  *unfolded = NULL;
  ast_t* def = (ast_t*)ast_data(type);

  // For type aliases, unfold to get the definition they name. ast_data on a
  // TK_TYPEALIASREF points to the TK_TYPE, not a trait.
  if((def != NULL) && (ast_id(type) == TK_TYPEALIASREF))
  {
    *unfolded = typealias_unfold(type);

    if((*unfolded != NULL) && (ast_id(*unfolded) == TK_NOMINAL))
      def = (ast_t*)ast_data(*unfolded);
    else
      def = NULL;
  }

  return def;
}

// Bring each trait or interface a literal provides directly to the expr pass,
// so its checked methods are there to inherit. Intersections and aliases are
// flattened; the parents of the provided traits are not walked. A trait whose
// check is running is not entered: the class waits for that check instead.
bool catch_up_provided(pass_opt_t* opt, ast_t* type)
{
  if(ast_id(type) == TK_ISECTTYPE)
  {
    for(ast_t* child = ast_child(type); child != NULL;
      child = ast_sibling(child))
    {
      if(!catch_up_provided(opt, child))
        return false;
    }

    return true;
  }

  ast_t* unfolded = NULL;
  ast_t* def = provided_def(type, &unfolded);
  bool ok = true;

  // While a default argument is checked, no trait check is started: a trait
  // check could need that same default. The trait is brought to the refer
  // pass, and its check waits for the class's drain.
  if((unfolded != NULL) && (ast_id(unfolded) == TK_ISECTTYPE))
    ok = catch_up_provided(opt, unfolded);
  else if((def != NULL) && !program_trait_check_in_progress(def))
    ok = ast_passes_type(&def, opt,
      program_any_default_check(def) ? PASS_REFER : PASS_EXPR);

  if(unfolded != NULL)
    ast_free_unattached(unfolded);

  return ok;
}

static bool catch_up_provides(pass_opt_t* opt, ast_t* provides)
{
  for(ast_t* child = ast_child(provides); child != NULL;
    child = ast_sibling(child))
  {
    if(!ast_passes_subtree(&child, opt, PASS_EXPR))
      return false;

    if(!catch_up_provided(opt, child))
      return false;
  }

  return true;
}

// provides_in_progress, recording in `visited` the traits walked, so a trait
// that several others provide is walked once.
static bool provides_in_progress_in(ast_t* type, astlist_t** visited)
{
  switch(ast_id(type))
  {
    case TK_PROVIDES:
    case TK_ISECTTYPE:
      for(ast_t* child = ast_child(type); child != NULL;
        child = ast_sibling(child))
      {
        if(provides_in_progress_in(child, visited))
          return true;
      }
      return false;

    case TK_NOMINAL:
    case TK_TYPEALIASREF:
    {
      ast_t* unfolded = NULL;
      ast_t* def = provided_def(type, &unfolded);
      bool r = false;

      if((unfolded != NULL) && (ast_id(unfolded) == TK_ISECTTYPE))
      {
        r = provides_in_progress_in(unfolded, visited);
      } else if((def != NULL) && ((ast_id(def) == TK_TRAIT) ||
        (ast_id(def) == TK_INTERFACE)) &&
        (astlist_find(*visited, def) == NULL)) {
        *visited = astlist_push(*visited, def);
        r = program_trait_check_in_progress(def) ||
          provides_in_progress_in(ast_childidx(def, 3), visited);
      }

      if(unfolded != NULL)
        ast_free_unattached(unfolded);

      return r;
    }

    default:
      return false;
  }
}

// Whether a trait whose check is running is among the types a provides list
// reaches, through intersections, aliases and the traits' own provides.
static bool provides_in_progress(ast_t* type)
{
  astlist_t* visited = NULL;
  bool r = provides_in_progress_in(type, &visited);
  astlist_free(visited);
  return r;
}

bool drain_deferred_classes(pass_opt_t* opt, ast_t* ast)
{
  if(program_draining(ast) || program_any_trait_check(ast))
    return true;

  program_set_draining(ast, true);
  bool ok = true;
  ast_t* def;

  // Classes made while the queued classes are checked join the queue and are
  // checked by this loop.
  while(ok && ((def = program_next_deferred(ast)) != NULL))
  {
    if(!catch_up_provides(opt, ast_childidx(def, 3)) ||
      !attach_trait_view_bodies(def, opt) ||
      !ast_passes_type(&def, opt, PASS_EXPR))
      ok = false;
  }

  program_set_draining(ast, false);
  return ok;
}

// The method a node is written in, or NULL.
static ast_t* enclosing_method(ast_t* ast)
{
  ast_t* method = ast_parent(ast);

  while((method != NULL) && (ast_id(method) != TK_FUN) &&
    (ast_id(method) != TK_BE) && (ast_id(method) != TK_NEW))
    method = ast_parent(method);

  return method;
}

// The body that holds a literal: the innermost iftype branch or method body
// around it, outside any array literal, or the parameter whose default holds
// it. Whether the literal's class can run depends only on these bodies, and
// the checks that rewrite nodes inside them, such as an array literal's, never
// free them; an array literal's own nodes may be freed and rebuilt.
static ast_t* literal_site(ast_t* ast)
{
  ast_t* start = ast;

  for(ast_t* p = ast_parent(ast); p != NULL; p = ast_parent(p))
  {
    token_id id = ast_id(p);

    if(id == TK_ARRAY)
      start = p;
    else if((id == TK_FUN) || (id == TK_BE) || (id == TK_NEW))
      break;
  }

  for(ast_t* body = start; ast_parent(body) != NULL; body = ast_parent(body))
  {
    ast_t* parent = ast_parent(body);

    switch(ast_id(parent))
    {
      case TK_FUN:
      case TK_BE:
      case TK_NEW:
        return (ast_childidx(parent, 7) == body) ? body : parent;

      case TK_IFTYPE:
        if(ast_childidx(parent, 2) == body)
          return body;
        break;

      case TK_IFTYPE_SET:
        if(ast_childidx(parent, 1) == body)
          return body;
        break;

      case TK_PARAM:
        return parent;

      default: {}
    }
  }

  return ast_parent(start);
}

// The file where a literal's text is written. A literal in a method that a
// type inherited is written in the file of the trait that supplied the
// method's body, not in the inheriting type's file.
static ast_t* literal_written_file(ast_t* ast)
{
  ast_t* method = enclosing_method(ast);

  if(method != NULL)
  {
    ast_t* donor = (ast_t*)ast_data(method);

    if(donor != NULL)
    {
      switch(ast_id(donor))
      {
        case TK_INTERFACE:
        case TK_TRAIT:
        case TK_PRIMITIVE:
        case TK_STRUCT:
        case TK_CLASS:
        case TK_ACTOR:
          return ast_nearest(donor, TK_FILE);

        default: {}
      }
    }
  }

  return ast_nearest(ast, TK_FILE);
}

static ast_t* new_literal_typeparam(ast_t* def, ast_t* t_params)
{
  ast_t* root = typeparam_root(def);

  for(ast_t* p = ast_child(t_params); p != NULL; p = ast_sibling(p))
  {
    if(typeparam_root((ast_t*)ast_data(p)) == root)
      return p;
  }

  return NULL;
}

static bool is_within(ast_t* ast, ast_t* outer)
{
  for(; ast != NULL; ast = ast_parent(ast))
  {
    if(ast == outer)
      return true;
  }

  return false;
}

// Point the references in an anonymous type's text to the type parameters it
// takes from its literal's enclosing type and method at the anonymous type's
// own parameters, which were made from those (each new parameter's data is
// the one it was made from). A narrowing that an iftype in that text declares
// is kept, and is made a narrowing of the new parameter.
static void remap_literal_typeparams(ast_t* ast, ast_t* literal,
  ast_t* t_params)
{
  if(ast == NULL)
    return;

  // A reference to a parameter declared outside the literal, as a type or as
  // a value, and a narrowing declared inside it of such a parameter.
  token_id id = ast_id(ast);
  ast_t* def = ((id == TK_TYPEPARAMREF) || (id == TK_TYPEPARAM) ||
    (id == TK_REFERENCE)) ? (ast_t*)ast_data(ast) : NULL;

  if((id == TK_REFERENCE) && (def != NULL) && (ast_id(def) != TK_TYPEPARAM))
    def = NULL;

  if((def != NULL) && (def != ast) &&
    ((id == TK_TYPEPARAM) || !is_within(def, literal)))
  {
    ast_t* p = new_literal_typeparam(def, t_params);
    if(p != NULL)
      ast_setdata(ast, p);
  }

  for(ast_t* child = ast_child(ast); child != NULL; child = ast_sibling(child))
    remap_literal_typeparams(child, literal, t_params);

  remap_literal_typeparams(ast_type(ast), literal, t_params);
}

// Whether a literal is in a method that its enclosing type inherited: the
// method's body comes from another type.
static bool literal_in_copy(ast_t* ast)
{
  ast_t* method = enclosing_method(ast);

  if(method == NULL)
    return false;

  ast_t* entity = method_entity(method);

  ast_t* donor = (ast_t*)ast_data(method);
  return (donor != NULL) && (entity != NULL) && (donor != entity);
}

// The annotation recording the kind of class a literal makes (see
// ANON_TRAIT_ANNOTATION in traits.h), or NULL for a literal in ordinary code.
static const char* literal_kind(ast_t* ast, pass_opt_t* opt)
{
  ast_t* method = enclosing_method(ast);

  if(method == NULL)
    return NULL;

  ast_t* entity = method_entity(method);

  if(entity == NULL)
    return NULL;

  // A literal in such a class's own text is of its class's kind.
  if(ast_has_annotation(entity, ANON_TRAIT_ANNOTATION, opt->strtab))
    return ANON_TRAIT_ANNOTATION;

  if(ast_has_annotation(entity, ANON_COPY_ANNOTATION, opt->strtab))
    return ANON_COPY_ANNOTATION;

  ast_t* donor = (ast_t*)ast_data(method);
  if((donor == NULL) ||
    ((ast_id(donor) != TK_TRAIT) && (ast_id(donor) != TK_INTERFACE)))
    return NULL;

  // Written in trait text: checked by the trait itself, or in a copy of the
  // method held by another type.
  return (donor == entity) ? ANON_TRAIT_ANNOTATION : ANON_COPY_ANNOTATION;
}

static bool mark_used_typeparams(ast_t* ast, ast_t* t_params, bool* used,
  ast_t* def)
{
  if(ast == NULL)
    return false;

  bool changed = false;
  token_id id = ast_id(ast);

  // The anonymous type's own type, such as its constructor's result, passes
  // its parameters on and doesn't count as mentioning them.
  if((id == TK_NOMINAL) && ((ast_data(ast) == def) ||
    (ast_name(ast_childidx(ast, 1)) == ast_name(ast_child(def)))))
    return false;

  if((id == TK_TYPEPARAMREF) || (id == TK_TYPEPARAM))
  {
    ast_t* ref_def = (ast_t*)ast_data(ast);
    size_t i = 0;

    for(ast_t* p = ast_child(t_params); p != NULL; p = ast_sibling(p), i++)
    {
      if((p == ref_def) && !used[i])
      {
        used[i] = true;
        changed = true;
      }
    }
  } else if((id == TK_REFERENCE) && (ast_data(ast) != NULL) &&
    (ast_id((ast_t*)ast_data(ast)) == TK_TYPEPARAM)) {
    // A value use (`A.create()`) bound where it is written to a type
    // parameter, which a body taken from another trait's method can name by
    // that method's name for it: matched by the parameter it is bound to.
    ast_t* root = typeparam_root((ast_t*)ast_data(ast));
    size_t i = 0;

    for(ast_t* p = ast_child(t_params); p != NULL; p = ast_sibling(p), i++)
    {
      if((typeparam_root((ast_t*)ast_data(p)) == root) && !used[i])
      {
        used[i] = true;
        changed = true;
      }
    }
  } else if(((id == TK_NOMINAL) && (ast_id(ast_child(ast)) == TK_NONE)) ||
    (id == TK_REFERENCE)) {
    // A name in literal text that the names or refer pass has not resolved
    // yet, as a type or as a value (`A.create()`).
    const char* name = (id == TK_NOMINAL) ?
      ast_name(ast_childidx(ast, 1)) : ast_name(ast_child(ast));
    size_t i = 0;

    for(ast_t* p = ast_child(t_params); p != NULL; p = ast_sibling(p), i++)
    {
      if((ast_name(ast_child(p)) == name) && !used[i])
      {
        used[i] = true;
        changed = true;
      }
    }
  }

  for(ast_t* child = ast_child(ast); child != NULL; child = ast_sibling(child))
  {
    if(mark_used_typeparams(child, t_params, used, def))
      changed = true;
  }

  if(mark_used_typeparams(ast_type(ast), t_params, used, def))
    changed = true;

  return changed;
}

// Remove the argument at `index` from every reference to the anonymous type
// itself in its own text, such as a constructor's result.
static void drop_self_typearg(ast_t* ast, ast_t* def, size_t index)
{
  if(ast == NULL)
    return;

  if((ast_id(ast) == TK_NOMINAL) && ((ast_data(ast) == def) ||
    (ast_name(ast_childidx(ast, 1)) == ast_name(ast_child(def)))))
  {
    ast_t* typeargs = ast_childidx(ast, 2);

    if(ast_id(typeargs) == TK_TYPEARGS)
    {
      ast_t* arg = ast_childidx(typeargs, index);
      if(arg != NULL)
        ast_remove(arg);

      if(ast_child(typeargs) == NULL)
        ast_setid(typeargs, TK_NONE);
    }
  }

  for(ast_t* child = ast_child(ast); child != NULL; child = ast_sibling(child))
    drop_self_typearg(child, def, index);

  drop_self_typearg(ast_type(ast), def, index);
}

// Keep only the collected type parameters the anonymous type's text mentions,
// with any parameter their constraints mention, and drop the matching type
// arguments of the call that makes it.
static void drop_unused_typeparams(ast_t* def, ast_t* t_params, ast_t* call)
{
  size_t count = ast_childcount(t_params);
  bool* used = (bool*)ponyint_pool_alloc_size(count * sizeof(bool));
  memset(used, 0, count * sizeof(bool));

  mark_used_typeparams(ast_childidx(def, 3), t_params, used, def);
  mark_used_typeparams(ast_childidx(def, 4), t_params, used, def);

  bool changed = true;
  while(changed)
  {
    changed = false;
    size_t i = 0;

    for(ast_t* p = ast_child(t_params); p != NULL; p = ast_sibling(p), i++)
    {
      if(used[i] &&
        mark_used_typeparams(ast_childidx(p, 1), t_params, used, def))
        changed = true;
    }
  }

  ast_t* qualify = ast_child(ast_child(call));
  ast_t* typeargs = (ast_id(qualify) == TK_QUALIFY) ?
    ast_childidx(qualify, 1) : NULL;

  // Remove from the end so the indices of the rest stay put.
  for(size_t i = count; i > 0; i--)
  {
    if(used[i - 1])
      continue;

    ast_t* p = ast_childidx(t_params, i - 1);
    ast_remove(p);
    drop_self_typearg(ast_childidx(def, 4), def, i - 1);

    if(typeargs != NULL)
      ast_remove(ast_childidx(typeargs, i - 1));
  }

  ponyint_pool_free_size(count * sizeof(bool), used);

  if(ast_child(t_params) == NULL)
  {
    ast_setid(t_params, TK_NONE);

    if(typeargs != NULL)
    {
      ast_t* type_ref = ast_child(qualify);
      ast_replace(&qualify, ast_dup(type_ref));
    }
  }
}

static void literal_declared_names(ast_t* ast, astlist_t** names)
{
  switch(ast_id(ast))
  {
    case TK_LET:
    case TK_VAR:
    case TK_PARAM:
    case TK_FVAR:
    case TK_FLET:
    case TK_EMBED:
    case TK_MATCH_CAPTURE:
    case TK_LAMBDACAPTURE:
      if(ast_id(ast_child(ast)) == TK_ID)
        *names = astlist_push(*names, ast_child(ast));
      break;

    default: {}
  }

  for(ast_t* child = ast_child(ast); child != NULL; child = ast_sibling(child))
    literal_declared_names(child, names);
}

static bool declared_in(astlist_t* names, const char* name)
{
  for(astlist_t* p = names; p != NULL; p = astlist_next(p))
  {
    if(ast_name(astlist_data(p)) == name)
      return true;
  }

  return false;
}

// Whether the literal's text uses a local, parameter or field from outside
// it, which the literal would capture.
static bool literal_captures(ast_t* ast, ast_t* literal, astlist_t* names)
{
  if(ast_id(ast) == TK_REFERENCE)
  {
    const char* name = ast_name(ast_child(ast));

    if(!is_name_dontcare(name) && !declared_in(names, name))
    {
      ast_t* def = ast_get(literal, name, NULL);

      if(def != NULL)
      {
        switch(ast_id(def))
        {
          case TK_LET:
          case TK_VAR:
          case TK_PARAM:
          case TK_FVAR:
          case TK_FLET:
          case TK_EMBED:
          case TK_MATCH_CAPTURE:
            return true;

          default: {}
        }
      }
    }
  }

  for(ast_t* child = ast_child(ast); child != NULL; child = ast_sibling(child))
  {
    if(literal_captures(child, literal, names))
      return true;
  }

  return false;
}

// Whether a lambda's types come from types written where it is: a method's
// result, a parameter's type (a default), a declaration's or array's written
// type. A lambda passed as an argument takes its types from the parameter of
// the method it is passed to, which an inheriting type can override.
static bool lambda_typed_where_written(ast_t* ast)
{
  ast_t* child = ast;
  ast_t* parent = ast_parent(ast);

  while((parent != NULL) && ((ast_id(parent) == TK_SEQ) ||
    (ast_id(parent) == TK_TUPLE)))
  {
    if((ast_id(parent) == TK_SEQ) && (ast_childlast(parent) != child))
      return false;

    child = parent;
    parent = ast_parent(parent);
  }

  if(parent == NULL)
    return false;

  switch(ast_id(parent))
  {
    case TK_FUN:
    case TK_BE:
    case TK_NEW:
    case TK_PARAM:
    case TK_ARRAY:
      return true;

    case TK_ASSIGN:
    {
      ast_t* left = ast_child(parent);
      return ((ast_id(left) == TK_LET) || (ast_id(left) == TK_VAR)) &&
        (ast_id(ast_childidx(left, 1)) != TK_NONE);
    }

    default:
      return false;
  }
}

// Whether the class a literal in trait text makes is fixed by the trait's
// text: it captures nothing, and nothing in it is inferred from the site
// except through types written in the trait. Such a literal makes one class,
// in its trait's own check, wherever it is expanded.
static bool literal_template_determined(ast_t* ast, bool typed_lambda)
{
  const char* nice = (const char*)ast_data(ast);

  // A location literal is made at each call.
  if((nice != NULL) && (strcmp(nice, "__loc") == 0))
    return false;

  // Fields, including the ones a lambda's capture list makes, are given
  // values when the object is made.
  ast_t* members = ast_childidx(ast, 2);
  for(ast_t* m = ast_child(members); m != NULL; m = ast_sibling(m))
  {
    switch(ast_id(m))
    {
      case TK_FVAR:
      case TK_FLET:
      case TK_EMBED:
        return false;

      default: {}
    }
  }

  astlist_t* names = NULL;
  literal_declared_names(ast, &names);
  bool captures = literal_captures(ast, ast, names);
  astlist_free(names);

  if(captures)
    return false;

  // A lambda has its nice name in the data field.
  return (nice == NULL) || typed_lambda || lambda_typed_where_written(ast);
}

// The type arguments of `target`, a trait `entity` provides directly or
// through other traits, as `entity` sees them. NULL if it doesn't.
static ast_t* provided_typeargs(ast_t* entity, ast_t* target, pass_opt_t* opt)
{
  ast_t* provides = ast_childidx(entity, 3);

  for(ast_t* ref = ast_child(provides); ref != NULL; ref = ast_sibling(ref))
  {
    if(ast_id(ref) != TK_NOMINAL)
      continue;

    ast_t* def = (ast_t*)ast_data(ref);
    if(def == NULL)
      continue;

    ast_t* typeargs = ast_childidx(ref, 2);

    if(def == target)
      return ast_dup(typeargs);

    ast_t* inner = provided_typeargs(def, target, opt);
    if(inner == NULL)
      continue;

    if(ast_id(inner) == TK_TYPEARGS)
      inner = reify(inner, ast_childidx(def, 1), typeargs, opt, false);

    return inner;
  }

  return NULL;
}

// In a type that inherited the method holding the literal, the class a
// template-determined literal made in its trait's own check, with the
// type arguments that site gives it. Returns the call that makes it, or NULL
// if the class isn't there. Sets `*failed` when the trait's check, started
// here, reported errors: the literal is then not built again.
static ast_t* reuse_trait_literal(pass_opt_t* opt, ast_t* ast, bool* failed)
{
  *failed = false;

  ast_t* method = enclosing_method(ast);

  if(method == NULL)
    return NULL;

  ast_t* entity = opt->check.frame->type;
  ast_t* trait = (ast_t*)ast_data(method);

  // A trait's own copy of a parent's default builds its own class.
  if((entity == NULL) || (ast_id(entity) == TK_TRAIT) ||
    (ast_id(entity) == TK_INTERFACE))
    return NULL;

  if((trait == NULL) ||
    ((ast_id(trait) != TK_TRAIT) && (ast_id(trait) != TK_INTERFACE)))
    return NULL;

  ast_t* origins = NULL;
  ast_t* def = program_literal_link(ast, ast, &origins);

  if((def == NULL) && !program_trait_check_in_progress(trait) &&
    !program_any_default_check(trait))
  {
    size_t before = errors_get_count(opt->check.errors);
    bool ok = ast_passes_type(&trait, opt, PASS_EXPR);
    program_add_unnoted_range(ast, before, errors_get_count(opt->check.errors));

    if(!ok)
    {
      *failed = true;
      return NULL;
    }

    def = program_literal_link(ast, ast, &origins);
  }

  if(def == NULL)
    return NULL;

  // A class whose constructor takes captured values is one the literal's
  // text didn't show to capture, such as one using a name that a lambda in it
  // also declares; this copy makes its own class, with its own captures.
  ast_t* create = ast_get(def, stringtab(opt->strtab, "create"), NULL);
  if((create == NULL) || (ast_childcount(ast_childidx(create, 3)) > 0))
    return NULL;

  ast_t* trait_args = provided_typeargs(entity, trait, opt);
  ast_t* trait_params = ast_childidx(trait, 1);
  ast_t* method_params = ast_childidx(method, 2);
  ast_t* typeargs = ast_from(ast, TK_NONE);

  for(ast_t* o = ast_child(origins); o != NULL; o = ast_sibling(o))
  {
    ast_t* param = (ast_t*)ast_data(o);
    ast_t* owner = ast_parent(param);
    ast_t* arg = NULL;

    if(owner == trait_params)
    {
      if((trait_args != NULL) && (ast_id(trait_args) == TK_TYPEARGS))
        arg = ast_dup(ast_childidx(trait_args, ast_index(param)));
    } else if(ast_id(method_params) == TK_TYPEPARAMS) {
      ast_t* own = ast_childidx(method_params, ast_index(param));

      if(own != NULL)
      {
        BUILD(ref, ast,
          NODE(TK_TYPEPARAMREF,
            ID(ast_name(ast_child(own)))
            NONE
            NONE));
        ast_setdata(ref, own);
        arg = ref;
      }
    }

    // A private type argument from another package keeps the site's own
    // class, where privacy is judged for that site.
    if((arg != NULL) && type_names_foreign_private(opt, arg))
    {
      ast_free_unattached(arg);
      arg = NULL;
    }

    if(arg == NULL)
    {
      ast_free_unattached(typeargs);
      if(trait_args != NULL)
        ast_free_unattached(trait_args);
      return NULL;
    }

    ast_append(typeargs, arg);
    ast_setid(typeargs, TK_TYPEARGS);
  }

  if(trait_args != NULL)
    ast_free_unattached(trait_args);

  BUILD(type_ref, ast, NODE(TK_REFERENCE, ID(ast_name(ast_child(def)))));
  ast_setdata(type_ref, def);

  if(ast_id(typeargs) != TK_NONE)
  {
    BUILD(t, ast, NODE(TK_QUALIFY, TREE(type_ref) TREE(typeargs)));
    type_ref = t;
  } else {
    ast_free_unattached(typeargs);
  }

  BUILD(call, ast,
    NODE(TK_CALL,
      NODE(TK_DOT,
        TREE(type_ref)
        ID("create"))
      NODE(TK_POSITIONALARGS)
      NONE
      NONE));

  // Reach applies the mutation exclusions of every site that makes the class.
  if(opt->mutate)
    ast_setflag(call, AST_FLAG_OBJECT_LITERAL);

  return call;
}

// Whether members, of an object literal or of the class made from one, hold
// no fields and no behaviours. A literal's written members show only its
// explicit captures.
static bool literal_without_state(ast_t* members)
{
  for(ast_t* m = ast_child(members); m != NULL; m = ast_sibling(m))
  {
    switch(ast_id(m))
    {
      case TK_FVAR:
      case TK_FLET:
      case TK_EMBED:
      case TK_BE:
        return false;

      default: {}
    }
  }

  return true;
}

// Whether two trees are written the same: the same tokens with the same
// text, and the same children.
static bool same_text(ast_t* a, ast_t* b)
{
  if((a == NULL) || (b == NULL))
    return a == b;

  if((ast_id(a) != ast_id(b)) ||
    (strcmp(ast_get_print(a), ast_get_print(b)) != 0))
    return false;

  ast_t* ca = ast_child(a);
  ast_t* cb = ast_child(b);

  for(; (ca != NULL) && (cb != NULL);
    ca = ast_sibling(ca), cb = ast_sibling(cb))
  {
    if(!same_text(ca, cb))
      return false;
  }

  return (ca == NULL) && (cb == NULL);
}

// The capability of the class a literal without state makes.
static token_id stateless_literal_cap(ast_t* cap)
{
  switch(ast_id(cap))
  {
    case TK_NONE:
    case TK_TAG:
    case TK_BOX:
    case TK_VAL:
      return TK_VAL;

    default:
      return ast_id(cap);
  }
}

// Whether `ast` has an identifier named as one of `typeparams`.
static bool names_typeparam_of(ast_t* ast, ast_t* typeparams)
{
  if((ast == NULL) || (ast_id(typeparams) != TK_TYPEPARAMS))
    return false;

  if(ast_id(ast) == TK_ID)
  {
    for(ast_t* p = ast_child(typeparams); p != NULL; p = ast_sibling(p))
    {
      if(ast_name(ast_child(p)) == ast_name(ast))
        return true;
    }
  }

  for(ast_t* c = ast_child(ast); c != NULL; c = ast_sibling(c))
  {
    if(names_typeparam_of(c, typeparams))
      return true;
  }

  return false;
}

// Whether the text of a literal in an inherited copy means the same in every
// type's copy: it names no type parameter of the trait the method came from
// or of the method, which each type's copy can give different arguments.
static bool literal_same_in_every_copy(ast_t* ast)
{
  ast_t* method = enclosing_method(ast);
  if(method == NULL)
    return false;

  ast_t* donor = (ast_t*)ast_data(method);
  if(donor == NULL)
    return false;

  ast_t* members = ast_childidx(ast, 2);
  return !names_typeparam_of(members, ast_childidx(donor, 1)) &&
    !names_typeparam_of(members, ast_childidx(method, 2));
}

// In an inherited copy, the call that makes the class another copy of the
// same literal made, when that class has no type parameters and the same
// provides and capability: the classes would be the same. NULL if there is
// none.
static ast_t* shared_copy_literal(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, provides, members);

  for(ast_t* def = program_next_copy_literal(ast, ast, NULL); def != NULL;
    def = program_next_copy_literal(ast, ast, def))
  {
    ast_t* create = ast_get(def, stringtab(opt->strtab, "create"), NULL);
    ast_t* def_provides = ast_child(ast_childidx(def, 3));

    if(ast_id(ast_childidx(def, 1)) != TK_NONE)
      continue;

    if((create == NULL) ||
      (ast_id(ast_child(create)) != stateless_literal_cap(cap)))
      continue;

    if(ast_id(provides) == TK_NONE)
    {
      if(def_provides != NULL)
        continue;
    } else if((def_provides == NULL) || (ast_sibling(def_provides) != NULL) ||
      !is_eqtype(def_provides, provides, NULL, opt)) {
      continue;
    }

    if(!same_text(members, program_copy_literal_text(ast, def)))
      continue;

    BUILD(type_ref, ast, NODE(TK_REFERENCE, ID(ast_name(ast_child(def)))));
    ast_setdata(type_ref, def);

    BUILD(call, ast,
      NODE(TK_CALL,
        NODE(TK_DOT,
          TREE(type_ref)
          ID("create"))
        NODE(TK_POSITIONALARGS)
        NONE
        NONE));

    if(opt->mutate)
      ast_setflag(call, AST_FLAG_OBJECT_LITERAL);

    return call;
  }

  return NULL;
}

bool expr_object(pass_opt_t* opt, ast_t** astp)
{
  ast_t* ast = *astp;
  bool ok = true;

  // A template-determined literal in an inherited copy is the class its
  // trait's own check made.
  const char* kind = literal_kind(ast, opt);
  bool template_determined = (kind != NULL) && literal_template_determined(ast,
    ast_has_annotation(ast, TYPED_LAMBDA_ANNOTATION, opt->strtab));

  // So is one in trait text whose class the trait's own check already made,
  // met again in a class made from another of the trait's literals.
  ast_t* unused_origins;
  bool already_linked = template_determined && (kind != NULL) &&
    (strcmp(kind, ANON_TRAIT_ANNOTATION) == 0) &&
    (program_literal_link(ast, ast, &unused_origins) != NULL);

  if(template_determined && (kind != NULL) &&
    ((strcmp(kind, ANON_COPY_ANNOTATION) == 0) || already_linked))
  {
    bool failed;
    ast_t* reused = reuse_trait_literal(opt, ast, &failed);

    if(failed)
      return false;

    if(reused != NULL)
    {
      ast_replace(astp, reused);

      if(ast_visit(astp, pass_syntax, NULL, opt, PASS_SYNTAX) != AST_OK)
        return false;

      return ast_passes_subtree(astp, opt, PASS_EXPR);
    }
  }

  AST_GET_CHILDREN(ast, cap, provides, members);

  // Another copy of the same literal may already have made a class this one
  // can share. A class made in code that can't run for its type isn't
  // reached, so one is neither shared from there nor shared into there.
  bool shareable = (kind != NULL) && !template_determined &&
    (strcmp(kind, ANON_COPY_ANNOTATION) == 0) &&
    ast_has_annotation(ast, LAMBDA_OBJECT_ANNOTATION, opt->strtab) &&
    literal_without_state(members) && literal_same_in_every_copy(ast) &&
    !is_in_unrunnable_code(ast, opt);
  ast_t* shared_text = NULL;

  if(shareable)
  {
    ast_t* shared = shared_copy_literal(opt, ast);

    if(shared != NULL)
    {
      ast_replace(astp, shared);

      if(ast_visit(astp, pass_syntax, NULL, opt, PASS_SYNTAX) != AST_OK)
        return false;

      return ast_passes_subtree(astp, opt, PASS_EXPR);
    }

    // The members as written, for a later copy to compare with.
    shared_text = ast_dup(members);
  }

  ast_clearflag(cap, AST_FLAG_PRESERVE);
  ast_clearflag(provides, AST_FLAG_PRESERVE);
  ast_clearflag(members, AST_FLAG_PRESERVE);

  ast_t* annotation = ast_consumeannotation(ast);

  // The anonymous type goes in the file where the literal is written, so the
  // names in its text resolve there, and takes its name from that module.
  ast_t* written_file = literal_written_file(ast);
  const char* c_id = module_hygienic_id_in(
    ast_nearest(written_file, TK_MODULE), opt);

  ast_t* t_params;
  ast_t* t_args;
  collect_type_params(ast, &t_params, &t_args, opt);

  // Each type argument refers to the parameter it passes on rather than
  // naming it: the literal's text can be in a method copied into a type whose
  // own scope gives the name another meaning.
  {
    ast_t* p = ast_child(t_params);
    ast_t* arg = ast_child(t_args);

    while((p != NULL) && (arg != NULL))
    {
      ast_t* next = ast_sibling(arg);
      ast_t* orig = (ast_t*)ast_data(p);
      BUILD(ref, arg,
        NODE(TK_TYPEPARAMREF,
          NICE_ID(ast_name(ast_child(orig)), ast_nice_name(ast_child(orig)))
          NONE
          NONE));
      ast_setdata(ref, orig);
      ast_replace(&arg, ref);
      arg = next;
      p = ast_sibling(p);
    }
  }

  if(opt->check.frame->iftype_body != NULL)
  {
    // Inside an iftype, a type argument names the narrowing of its parameter
    // that is in scope here.
    for(ast_t* arg = ast_child(t_args); arg != NULL; arg = ast_sibling(arg))
      ast_setdata(arg, typeparam_resolve(arg, ast, opt->strtab));

    for(ast_t* tp = ast_child(t_params); tp != NULL; tp = ast_sibling(tp))
    {
      const char* tp_name = ast_name(ast_child(tp));
      ast_t* current_def = ast_get(ast, tp_name, NULL);

      if((current_def != NULL) && (ast_id(current_def) == TK_TYPEPARAM) &&
        (ast_data(current_def) != NULL) &&
        ((ast_t*)ast_data(current_def) != current_def))
      {
        ast_t* narrowed = sanitise_type(ast_childidx(current_def, 1), opt);
        ast_t* old_constraint = ast_childidx(tp, 1);
        ast_swap(old_constraint, narrowed);
        ast_free(old_constraint);
      }
    }
  }

  if(ast_id(t_params) == TK_TYPEPARAMS)
  {
    // In a method a type inherited, the inheriting type's own parameters were
    // not written where the literal is. Give their copies names no source can
    // write, so they can't clash with a name the literal declares; everything
    // that refers to them does so by identity.
    if(literal_in_copy(ast))
    {
      ast_t* module = ast_nearest(written_file, TK_MODULE);

      for(ast_t* p = ast_child(t_params); p != NULL; p = ast_sibling(p))
      {
        ast_t* orig = (ast_t*)ast_data(p);
        ast_t* owner = ast_parent(ast_parent(orig));

        if((owner != NULL) && (ast_id(owner) != TK_FUN) &&
          (ast_id(owner) != TK_BE) && (ast_id(owner) != TK_NEW))
        {
          ast_t* id = ast_child(p);
          const char* nice = ast_nice_name(id);
          ast_set_name(id, module_hygienic_id_in(module, opt), opt->strtab);
          ast_set_nice_name(id, nice, opt->strtab);
        }
      }
    }
  }

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

  if(kind != NULL)
    ast_add_internal_annotation(def, kind, opt->strtab);

  if((kind != NULL) && (strcmp(kind, ANON_COPY_ANNOTATION) == 0))
    program_set_anon_host(def, opt->check.frame->type,
      opt->check.frame->method, literal_site(ast));

  // We will have a create method in the type.
  BUILD(create, members,
    NODE(TK_NEW, AST_SCOPE
      NONE
      ID("create")
      NONE
      NODE(TK_PARAMS)
      NONE
      NONE
      NONE
      NODE(TK_SEQ,
        NODE(TK_TRUE))
      NONE));

  // The anonymous type can be in another module or package than the
  // expression that creates it, so the reference names its definition
  // instead of being looked up by name.
  BUILD(type_ref, ast, NODE(TK_REFERENCE, ID(c_id)));
  ast_setdata(type_ref, def);

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
  ast_t* create_body = ast_childidx(create, 7);
  ast_t* call_args = ast_childidx(call, 1);
  ast_t* class_members = ast_childidx(def, 4);
  ast_t* member = ast_child(members);

  if(opt->mutate)
    ast_setflag(call, AST_FLAG_OBJECT_LITERAL);

  bool has_fields = false;
  bool has_behaviours = false;
  uint16_t exclusions = opt->mutate ? ast_mutation_exclusions(ast) : 0;

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

      default:
      {
        has_behaviours = has_behaviours || (ast_id(member) == TK_BE);

        // The literal is freed when it is replaced below. A closed copy keeps
        // the method's references to its own nodes, such as to a narrowing
        // that an iftype in its body declares, pointing into the copy.
        ast_t* method = ast_append(class_members, ast_dup_closed(member));

        if(opt->mutate)
        {
          ast_setflag(method, AST_FLAG_OBJECT_LITERAL);
          if(ast_id(method) == TK_METHODGROUP)
          {
            for(ast_t* m = ast_child(method); m != NULL; m = ast_sibling(m))
              ast_setflag(m, AST_FLAG_OBJECT_LITERAL);
          }
        }

        // The generated type is outside the literal's enclosing scope.
        ast_exclude_mutations(method, exclusions);

        break;
      }
    }

    member = ast_sibling(member);
  }

  // Add the create function at the end.
  ast_append(class_members, create);

  // Add new type to the written file and bring it up to date with passes.
  ast_append(written_file, def);

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

  // The anonymous type's text, including the fields that hold its captures,
  // refers to the type parameters it takes from its enclosing type and method
  // by identity. Point it at the anonymous type's own parameters.
  if(ast_id(t_params) == TK_TYPEPARAMS)
  {
    remap_literal_typeparams(ast_childidx(def, 3), def, t_params);
    remap_literal_typeparams(ast_childidx(def, 4), def, t_params);

    // A literal takes only the type parameters it mentions.
    drop_unused_typeparams(def, t_params, call);
  }

  // The class a template-determined literal makes in its trait's own check is
  // the one every inheriting site uses. Record which parameters it was made
  // from.
  if(template_determined && (kind != NULL) &&
    (strcmp(kind, ANON_TRAIT_ANNOTATION) == 0) && !already_linked)
  {
    ast_t* origins = ast_from(ast, TK_TYPEPARAMS);

    if(ast_id(t_params) == TK_TYPEPARAMS)
    {
      for(ast_t* p = ast_child(t_params); p != NULL; p = ast_sibling(p))
      {
        BUILD(o, ast,
          NODE(TK_TYPEPARAMREF, ID(ast_name(ast_child(p))) NONE NONE));
        ast_setdata(o, ast_data(p));
        ast_append(origins, o);
      }
    }

    program_set_literal_link(ast, def, origins);
  }

  if(ast_id(t_params) == TK_TYPEPARAMS)
    own_collected_typeparams(t_params);

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
    pony_assert(!ast_has_annotation(def, "ponyint_bare", opt->strtab));

  // Later copies of this literal can share a class with no state and no type
  // parameters. Captures the literal doesn't write become fields only when
  // the class is made, so its fields are checked here.
  if(shareable && (ast_id(t_params) != TK_TYPEPARAMS) &&
    literal_without_state(ast_childidx(def, 4)))
    program_add_copy_literal(ast, def, shared_text);
  else
    ast_free(shared_text);

  // Reset constructor to pick up the correct defaults.
  ast_setid(ast_child(create), cap_id);
  ast_t* result = ast_childidx(create, 4);
  ast_replace(&result,
    type_for_class(opt, def, result, cap_id, TK_EPHEMERAL, false));

  // Catch up provides before catching up the entire type.
  if(!catch_up_provides(opt, provides))
    return false;

  // A class made from trait text that provides a trait whose check is running,
  // or made while a default argument is checked, takes its inherited bodies
  // when those checks have finished.
  bool defer = program_any_default_check(ast) ||
    ((kind != NULL) && program_any_trait_check(ast) &&
      provides_in_progress(provides));
  if(defer)
    program_defer_class(def);

  // Replace object..end with $0.create(...) before type-checking the
  // anonymous type. This must happen first so that if the anonymous type
  // inherits methods from its provided traits (during PASS_TRAITS), those
  // methods see the replacement call rather than the raw object literal
  // (whose children have had PRESERVE cleared and haven't been through
  // name resolution). Process the call through PASS_REFER so inherited
  // copies have resolved references; PASS_EXPR is deferred until after
  // the anonymous type is fully processed.
  ast_replace(astp, call);

  if(ast_visit(astp, pass_syntax, NULL, opt, PASS_SYNTAX) != AST_OK)
    return false;

  if(!ast_passes_subtree(astp, opt, PASS_REFER))
    return false;

  // Type check the anonymous type, or, if it waits, bring it to the refer pass
  // so the call to it can be checked now.
  if(!ast_passes_type(&def, opt, defer ? PASS_REFER : PASS_EXPR))
    return false;

  // Finish processing the replacement call through the expression pass, now
  // that the anonymous type is type-checked, or brought to the refer pass
  // when its check waits for a trait's.
  if(!ast_passes_subtree(astp, opt, PASS_EXPR))
    return false;

  return ok;
}
