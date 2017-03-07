#include "lambda.h"
#include "literal.h"
#include "reference.h"
#include "../ast/astbuild.h"
#include "../ast/printbuf.h"
#include "../pass/pass.h"
#include "../pass/expr.h"
#include "../pass/syntax.h"
#include "../type/alias.h"
#include "../type/sanitise.h"
#include "../pkg/package.h"
#include "ponyassert.h"


// Process the given capture and create the AST for the corresponding field.
// Returns the create field AST, which must be freed by the caller.
// Returns NULL on error.
static ast_t* make_capture_field(pass_opt_t* opt, ast_t* capture)
{
  pony_assert(capture != NULL);

  AST_GET_CHILDREN(capture, id_node, type, value);
  const char* name = ast_name(id_node);

  // There are 3 varieties of capture:
  // x -> capture variable x, type from defn of x
  // x = y -> capture expression y, type inferred from expression type
  // x: T = y -> capture expression y, type T

  if(ast_id(value) == TK_NONE)
  {
    // Variable capture
    pony_assert(ast_id(type) == TK_NONE);

    ast_t* def = ast_get(capture, name, NULL);

    if(def == NULL)
    {
      ast_error(opt->check.errors, id_node,
        "cannot capture \"%s\", variable not defined", name);
      return NULL;
    }

    // lambda captures used before their declaration with their type
    // not defined are not legal
    if(!def_before_use(opt, def, capture, name))
      return NULL;

    token_id def_id = ast_id(def);

    if(def_id != TK_ID && def_id != TK_FVAR && def_id != TK_FLET &&
      def_id != TK_PARAM)
    {
      ast_error(opt->check.errors, id_node, "cannot capture \"%s\", can only "
        "capture fields, parameters and local variables", name);
      return NULL;
    }

    BUILD(capture_rhs, id_node, NODE(TK_REFERENCE, ID(name)));

    type = alias(ast_type(def));
    value = capture_rhs;
  } else if(ast_id(type) == TK_NONE) {
    // No type specified, use type of the captured expression
    if(ast_type(value) == NULL)
      return NULL;
    type = alias(ast_type(value));
  } else {
    // Type given, infer literals
    if(!coerce_literals(&value, type, opt))
      return NULL;
  }

  if(is_typecheck_error(type))
    return NULL;

  type = sanitise_type(type);

  BUILD(field, id_node,
    NODE(TK_FVAR,
      TREE(id_node)
      TREE(type)
      TREE(value)));

  return field;
}


bool expr_lambda(pass_opt_t* opt, ast_t** astp)
{
  pony_assert(astp != NULL);
  ast_t* ast = *astp;
  pony_assert(ast != NULL);

  AST_GET_CHILDREN(ast, receiver_cap, name, t_params, params, captures,
    ret_type, raises, body, reference_cap);
  ast_t* annotation = ast_consumeannotation(ast);

  ast_t* members = ast_from(ast, TK_MEMBERS);
  ast_t* last_member = NULL;
  bool failed = false;

  // Process captures
  for(ast_t* p = ast_child(captures); p != NULL; p = ast_sibling(p))
  {
    ast_t* field = make_capture_field(opt, p);

    if(field != NULL)
      ast_list_append(members, &last_member, field);
    else  // An error occurred, just keep going to potentially find more errors
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
      NONE    // Doc string
      NONE)); // Guard

  ast_list_append(members, &last_member, apply);
  ast_setflag(members, AST_FLAG_PRESERVE);

  printbuf_t* buf = printbuf_new();
  printbuf(buf, "{(");
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
      TREE(reference_cap)
      NONE  // Provides list
      TREE(members)));

  printbuf_free(buf);

  // Catch up passes
  if(ast_visit(astp, pass_syntax, NULL, opt, PASS_SYNTAX) != AST_OK)
    return false;

  return ast_passes_subtree(astp, opt, PASS_EXPR);
}


static bool capture_free_variable(pass_opt_t* opt, ast_t* ctx, ast_t* def,
  ast_t* ast)
{
  const char* name = ast_name(ast_child(ast));

  sym_status_t status;
  ast_t* refdef = ast_get(ast, name, &status);

  if(refdef != NULL)
    return true;

  refdef = ast_get(ctx, name, &status);

  if(refdef == NULL)
  {
    ast_error(opt->check.errors, ast,
      "cannot capture \"%s\", variable not defined", name);
    return false;
  }

  if(!def_before_use(opt, refdef, ctx, name))
    return false;

  token_id def_id = ast_id(refdef);

  if(def_id != TK_ID && def_id != TK_FVAR && def_id != TK_FLET &&
    def_id != TK_PARAM)
  {
    ast_error(opt->check.errors, ast, "cannot capture \"%s\", can only "
      "capture fields, parameters and local variables", name);
    return false;
  }

  // TODO: check if we've already captured it

  // TODO: add as a field, add as a constructor parameter, init field from
  // parameter in constructor, add at the call site
  ast_t* type = alias(ast_type(refdef));

  if(is_typecheck_error(type))
    return false;

  type = sanitise_type(type);

  (void)def;

  printf("CAPTURE %s\n", name);

  ast_t* p_id = ast_from_string(ast, package_hygienic_id(&opt->check));

  BUILD(field, def,
    NODE(TK_FVAR,
      ID(name)
      TREE(type)
      NONE));

  BUILD(param, def,
    NODE(TK_PARAM,
      TREE(p_id)
      TREE(type)
      NONE));

  BUILD(arg, ctx, NODE(TK_REFERENCE, ID(name)));

  BUILD(assign, def,
    NODE(TK_ASSIGN,
      NODE(TK_CONSUME, NODE(TK_NONE) NODE(TK_REFERENCE, TREE(p_id)))
      NODE(TK_REFERENCE, ID(name))));

  return true;
}


static bool capture_free_variables(pass_opt_t* opt, ast_t* ctx, ast_t* def,
  ast_t* ast)
{
  // Skip preserved ASTs.
  if(ast_checkflag(ast, AST_FLAG_PRESERVE))
    return true;

  bool ok = true;

  if(ast_id(ast) == TK_REFERENCE)
  {
    // Try to capture references.
    if(!capture_free_variable(opt, ctx, def, ast))
      ok = false;
  } else {
    // Proceed down through all child ASTs.
    for(ast_t* p = ast_child(ast); p != NULL; p = ast_sibling(p))
    {
      if(!capture_free_variables(opt, ctx, def, p))
        ok = false;
    }
  }

  return ok;
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
      NONE
      NONE
      NONE
      NODE(TK_SEQ)
      NONE
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
      NONE
      NONE
      NODE(TK_DOT,
        TREE(type_ref)
        ID("create"))));

  ast_t* create_params = ast_childidx(create, 3);
  ast_t* create_body = ast_childidx(create, 6);
  ast_t* call_args = ast_child(call);
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
        AST_GET_CHILDREN(member, id, type, init);
        ast_t* p_id = ast_from_string(id, package_hygienic_id(&opt->check));

        // The field is: var/let/embed id: type
        BUILD(field, member,
          NODE(ast_id(member),
            TREE(id)
            TREE(type)
            NONE));

        // The param is: $0: type
        BUILD(param, member,
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
            NODE(TK_CONSUME, NODE(TK_NONE) NODE(TK_REFERENCE, TREE(p_id)))
            NODE(TK_REFERENCE, TREE(id))));

        ast_setid(create_params, TK_PARAMS);
        ast_setid(call_args, TK_POSITIONALARGS);

        ast_append(class_members, field);
        ast_append(create_params, param);
        ast_append(create_body, assign);
        ast_append(call_args, arg);

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

  if(!has_fields)
  {
    // End the constructor with 'true', since it has no parameters.
    BUILD(true_node, ast, NODE(TK_TRUE));
    ast_append(create_body, true_node);
  }

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
  }
  else if(!has_fields && (cap_id == TK_NONE || cap_id == TK_TAG ||
    cap_id == TK_BOX || cap_id == TK_VAL))
  {
    // Change the type from a class to a primitive.
    ast_setid(def, TK_PRIMITIVE);
  }
  else
  {
    // Type is a class, set the create capability as specified
    ast_setid(ast_child(create), cap_id);
  }

  // Add the create function at the end.
  ast_append(class_members, create);

  // Replace object..end with $0.create(...)
  ast_t* module = ast_nearest(ast, TK_MODULE);
  ast_replace(astp, call);

  // Add new type to current module and bring it up to date with passes.
  ast_append(module, def);

  // Turn any free variables into fields.
  if(!ast_passes_type(&def, opt, PASS_SCOPE))
    return false;

  members = ast_childidx(def, 4);

  for(ast_t* p = ast_child(members); p != NULL; p = ast_sibling(p))
  {
    switch(ast_id(p))
    {
      case TK_FUN:
      case TK_BE:
      {
        if(!capture_free_variables(opt, *astp, def, p))
          ok = false;
        break;
      }

      default: {}
    }
  }

  // Reset the scope and rewind the pass.
  ast_clear(def);
  ast_resetpass(def, PASS_SUGAR);

  // TODO: something about parameterised type aliases goes wrong
  // nothing to do with the scope stuff above

  // Type check the anonymous type.
  if(!ast_passes_type(&def, opt, PASS_EXPR))
    return false;

  // Catch up passes
  if(ast_visit(astp, pass_syntax, NULL, opt, PASS_SYNTAX) != AST_OK)
    return false;

  if(!ast_passes_subtree(astp, opt, PASS_EXPR))
    return false;

  return ok;
}
