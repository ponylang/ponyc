#include "array.h"
#include "call.h"
#include "control.h"
#include "literal.h"
#include "postfix.h"
#include "reference.h"
#include "../ast/astbuild.h"
#include "../pkg/package.h"
#include "../pass/names.h"
#include "../pass/expr.h"
#include "../pass/refer.h"
#include "../type/alias.h"
#include "../type/assemble.h"
#include "../type/reify.h"
#include "../type/subtype.h"
#include "../type/lookup.h"
#include "ponyassert.h"

static ast_t* build_array_type(pass_opt_t* opt, ast_t* scope, ast_t* elem_type,
  token_id cap, token_id eph)
{
  elem_type = ast_dup(elem_type);

  BUILD(typeargs, elem_type,
    NODE(TK_TYPEARGS, TREE(elem_type)));

  ast_t* array_type = type_builtin_args(opt, scope, "Array", typeargs);
  ast_setid(ast_childidx(array_type, 3), cap);
  ast_setid(ast_childidx(array_type, 4), eph);

  return array_type;
}

static ast_t* strip_this_arrow(pass_opt_t* opt, ast_t* ast)
{
  if((ast_id(ast) == TK_ARROW) &&
    (ast_id(ast_child(ast)) == TK_THISTYPE))
    return ast_childidx(ast, 1);

  if(ast_id(ast) == TK_TUPLETYPE)
  {
    ast_t* new_ast = ast_from(ast, TK_TUPLETYPE);

    for(ast_t* c = ast_child(ast); c != NULL; c = ast_sibling(c))
      ast_append(new_ast, strip_this_arrow(opt, c));

    return new_ast;
  }

  return ast;
}

static ast_t* detect_apply_element_type(pass_opt_t* opt, ast_t* ast, ast_t* def)
{
  // The interface must have an apply method for us to find it.
  ast_t* apply = ast_get(def, stringtab("apply"), NULL);
  if((apply == NULL) || (ast_id(apply) != TK_FUN))
    return NULL;

  // The apply method must match the signature we're expecting.
  AST_GET_CHILDREN(apply, receiver_cap, apply_name, type_params, params,
    ret_type, question);
  if((ast_id(receiver_cap) != TK_BOX) ||
    (ast_id(type_params) != TK_NONE) ||
    (ast_childcount(params) != 1) ||
    (ast_id(question) != TK_QUESTION))
    return NULL;

  ast_t* param = ast_child(params);
  ast_t* param_type = ast_childidx(param, 1);
  if(ast_name(ast_childidx(param_type, 1)) != stringtab("USize"))
    return NULL;

  // Based on the return type we try to figure out the element type.
  ast_t* elem_type = ret_type;

  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* typeargs = ast_childidx(ast, 2);
  if(ast_id(typeparams) == TK_TYPEPARAMS)
    elem_type = reify(elem_type, typeparams, typeargs, opt, true);

  return strip_this_arrow(opt, elem_type);
}

static ast_t* detect_values_element_type(pass_opt_t* opt, ast_t* ast,
  ast_t* def)
{
  // The interface must have an apply method for us to find it.
  ast_t* values = ast_get(def, stringtab("values"), NULL);
  if((values == NULL) || (ast_id(values) != TK_FUN))
    return NULL;

  // The values method must match the signature we're expecting.
  AST_GET_CHILDREN(values, receiver_cap, apply_name, type_params, params,
    ret_type, question);
  if((ast_id(receiver_cap) != TK_BOX) ||
    (ast_id(type_params) != TK_NONE) ||
    (ast_childcount(params) != 0) ||
    (ast_id(question) != TK_NONE))
    return NULL;

  if((ast_id(ret_type) != TK_NOMINAL) ||
    (ast_name(ast_childidx(ret_type, 1)) != stringtab("Iterator")) ||
    (ast_childcount(ast_childidx(ret_type, 2)) != 1))
    return NULL;

  // Based on the return type we try to figure out the element type.
  ast_t* elem_type = ast_child(ast_childidx(ret_type, 2));

  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* typeargs = ast_childidx(ast, 2);
  if(ast_id(typeparams) == TK_TYPEPARAMS)
    elem_type = reify(elem_type, typeparams, typeargs, opt, true);

  if((ast_id(elem_type) == TK_ARROW) &&
    (ast_id(ast_child(elem_type)) == TK_THISTYPE))
    elem_type = ast_childidx(elem_type, 1);

  return elem_type;
}

static void find_possible_element_types(pass_opt_t* opt, ast_t* ast,
  astlist_t** list)
{
  switch(ast_id(ast))
  {
    case TK_NOMINAL:
    {
      AST_GET_CHILDREN(ast, package, name, typeargs, cap, eph);

      // If it's an actual Array type, note it as a possibility and move on.
      if(stringtab("Array") == ast_name(name))
      {
        *list = astlist_push(*list, ast_child(typeargs));
        return;
      }

      // Otherwise, an Array-matching type must be an interface.
      ast_t* def = (ast_t*)ast_data(ast);
      if((def == NULL) || (ast_id(def) != TK_INTERFACE))
        return;

      // Try to find an element type by looking for an apply method.
      ast_t* elem_type = detect_apply_element_type(opt, ast, def);

      // Otherwise, try to find an element type by looking for a values method.
      if(elem_type == NULL)
        elem_type = detect_values_element_type(opt, ast, def);

      // If we haven't found an element type by now, give up.
      if(elem_type == NULL)
        return;

      // Construct a guess of the corresponding Array type.
      // Use iso^ so that the object cap isn't a concern for subtype checking.
      ast_t* array_type = build_array_type(opt, ast, elem_type, TK_ISO, TK_EPHEMERAL);

      // The guess type must be a subtype of the interface type.
      if(!is_subtype(array_type, ast, NULL, opt))
      {
        ast_free_unattached(array_type);
        return;
      }

      ast_free_unattached(array_type);

      // Note this as a possible element type and move on.
      *list = astlist_push(*list, elem_type);
      return;
    }

    case TK_ARROW:
      find_possible_element_types(opt, ast_childidx(ast, 1), list);
      return;

    case TK_TYPEPARAMREF:
    {
      ast_t* def = (ast_t*)ast_data(ast);
      pony_assert(ast_id(def) == TK_TYPEPARAM);
      find_possible_element_types(opt, ast_childidx(def, 1), list);
      return;
    }

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      for(ast_t* c = ast_child(ast); c != NULL; c = ast_sibling(c))
        find_possible_element_types(opt, c, list);
      return;
    }

    default:
      break;
  }
}

static void find_possible_iterator_element_types(pass_opt_t* opt, ast_t* ast,
  astlist_t** list)
{
  switch(ast_id(ast))
  {
    case TK_NOMINAL:
    {
      AST_GET_CHILDREN(ast, package, name, typeargs, cap, eph);

      if(stringtab("Iterator") == ast_name(name))
      {
        *list = astlist_push(*list, ast_child(typeargs));
      }
      return;
    }

    case TK_ARROW:
      find_possible_iterator_element_types(opt, ast_childidx(ast, 1), list);
      return;

    case TK_TYPEPARAMREF:
    {
      ast_t* def = (ast_t*)ast_data(ast);
      pony_assert(ast_id(def) == TK_TYPEPARAM);
      find_possible_iterator_element_types(opt, ast_childidx(def, 1), list);
      return;
    }

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      for(ast_t* c = ast_child(ast); c != NULL; c = ast_sibling(c))
        find_possible_iterator_element_types(opt, c, list);
    }

    default:
      break;
  }
}

static bool infer_element_type(pass_opt_t* opt, ast_t* ast,
  ast_t** type_spec_p, ast_t* antecedent_type)
{
  astlist_t* possible_element_types = NULL;

  if(antecedent_type != NULL)
  {
    // List the element types of all array-matching types in the antecedent.
    find_possible_element_types(opt, antecedent_type, &possible_element_types);
  } else {
    // If the ast parent is a call to values() and the antecedent of that call
    // is an Iterator, then we can get possible element types that way.
    if((ast_id(ast_parent(ast)) == TK_DOT) &&
      (ast_name(ast_sibling(ast)) == stringtab("values")))
    {
      ast_t* dot = ast_parent(ast);
      antecedent_type = find_antecedent_type(opt, dot, NULL);
      if (antecedent_type != NULL)
        find_possible_iterator_element_types(opt, antecedent_type,
          &possible_element_types);
    }
  }

  // If there's more than one possible element type, remove equivalent types.
  if(astlist_length(possible_element_types) > 1)
  {
    astlist_t* new_list = NULL;

    astlist_t* left_cursor = possible_element_types;
    for(; left_cursor != NULL; left_cursor = astlist_next(left_cursor))
    {
      bool eqtype_of_any_remaining = false;

      astlist_t* right_cursor = astlist_next(left_cursor);
      for(; right_cursor != NULL; right_cursor = astlist_next(right_cursor))
      {
        ast_t* left = astlist_data(left_cursor);
        ast_t* right = astlist_data(right_cursor);
        if((right_cursor == left_cursor) || is_eqtype(right, left, NULL, opt))
          eqtype_of_any_remaining = true;
      }

      if(!eqtype_of_any_remaining)
        new_list = astlist_push(new_list, astlist_data(left_cursor));
    }

    astlist_free(possible_element_types);
    possible_element_types = new_list;
  }

  // If there's still more than one possible element type, choose the most
  // specific type (removing types that are supertypes of one or more others).
  if(astlist_length(possible_element_types) > 1)
  {
    astlist_t* new_list = NULL;

    astlist_t* super_cursor = possible_element_types;
    for(; super_cursor != NULL; super_cursor = astlist_next(super_cursor))
    {
      bool supertype_of_any = false;

      astlist_t* sub_cursor = possible_element_types;
      for(; sub_cursor != NULL; sub_cursor = astlist_next(sub_cursor))
      {
        if((sub_cursor != super_cursor) && is_subtype(astlist_data(sub_cursor),
          astlist_data(super_cursor), NULL, opt))
          supertype_of_any = true;
      }

      if(!supertype_of_any)
        new_list = astlist_push(new_list, astlist_data(super_cursor));
    }

    astlist_free(possible_element_types);
    possible_element_types = new_list;
  }

  // If there's still more than one possible type, test against the elements,
  // creating a new list containing only the possibilities that are supertypes.
  if(astlist_length(possible_element_types) > 1)
  {
    astlist_t* new_list = NULL;

    astlist_t* cursor = possible_element_types;
    for(; cursor != NULL; cursor = astlist_next(cursor))
    {
      ast_t* cursor_type = consume_type(astlist_data(cursor), TK_NONE);

      bool supertype_of_all = true;
      ast_t* elem = ast_child(ast_childidx(ast, 1));
      for(; elem != NULL; elem = ast_sibling(elem))
      {
        // Catch up the elem to the expr pass, so we can get its type.
        if(ast_visit(&elem, pass_pre_expr, pass_expr, opt, PASS_EXPR) != AST_OK)
          return false;

        ast_t* elem_type = ast_type(elem);
        if(is_typecheck_error(elem_type) || ast_id(elem_type) == TK_LITERAL)
          break;
        // defensive-only NULL test, no cursor type should be ephemeral
        if(cursor_type != NULL && !is_subtype(elem_type, cursor_type, NULL, opt))
          supertype_of_all = false;

      }

      if(supertype_of_all)
        new_list = astlist_push(new_list, astlist_data(cursor));

      ast_free_unattached(cursor_type);
    }

    astlist_free(possible_element_types);
    possible_element_types = new_list;
  }

  // If there's exactly one possible element type remaining, use it.
  if(astlist_length(possible_element_types) == 1)
    ast_replace(type_spec_p, astlist_data(possible_element_types));

  return true;
}

ast_result_t expr_pre_array(pass_opt_t* opt, ast_t** astp)
{
  ast_t* ast = *astp;

  pony_assert(ast_id(ast) == TK_ARRAY);
  AST_GET_CHILDREN(ast, type_spec, elements);

  // Try to find an antecedent type, or bail out if none was found.
  bool is_recovered = false;
  ast_t* antecedent_type = find_antecedent_type(opt, ast, &is_recovered);

  // If we don't have an explicit element type, try to infer it.
  if(ast_id(type_spec) == TK_NONE)
  {
    if(!infer_element_type(opt, ast, &type_spec, antecedent_type))
      return AST_ERROR;
  }

  // If we still don't have an element type, bail out.
  if(ast_id(type_spec) == TK_NONE)
    return AST_OK;

  // If there is no recover statement between the antecedent type and here,
  // and if the array literal is not a subtype of the antecedent type,
  // but would be if the object cap were ignored, then recover it.
  ast_t* array_type = build_array_type(opt, ast, type_spec, TK_REF, TK_NONE);
  if((antecedent_type != NULL) && !is_recovered &&
    !is_subtype(array_type, antecedent_type, NULL, opt) &&
    is_subtype_ignore_cap(array_type, antecedent_type, NULL, opt))
  {
    ast_free_unattached(array_type);

    BUILD(recover, ast,
      NODE(TK_RECOVER,
        NODE(TK_ISO)
        NODE(TK_SEQ, AST_SCOPE
          TREE(ast))));

    ast_replace(astp, recover);

    // Run the expr pass on this recover block.
    if(ast_visit(astp, pass_pre_expr, pass_expr, opt, PASS_EXPR) != AST_OK)
      return AST_ERROR;

    // We've already processed the expr pass for the array, so ignore it now.
    return AST_IGNORE;
  }

  ast_free_unattached(array_type);
  return AST_OK;
}

bool expr_array(pass_opt_t* opt, ast_t** astp)
{
  ast_t* ast = *astp;
  ast_t* type = NULL;
  bool told_type = false;

  pony_assert(ast_id(ast) == TK_ARRAY);
  AST_GET_CHILDREN(ast, type_spec, elements);
  size_t size = ast_childcount(elements);

  if(ast_id(type_spec) != TK_NONE)
  {
    type = type_spec;
    told_type = true;
  }

  if(!told_type && (size == 0))
  {
    ast_error(opt->check.errors, ast, "an empty array literal must specify "
      "the element type or it must be inferable from context");
    return false;
  }

  for(ast_t* ele = ast_child(elements); ele != NULL; ele = ast_sibling(ele))
  {
    if(ast_checkflag(ele, AST_FLAG_JUMPS_AWAY))
    {
      ast_error(opt->check.errors, ele,
          "an array can't contain an expression that jumps away with no value");
      ast_free_unattached(type);
      return false;
    }

    ast_t* c_type = ast_type(ele);
    if(is_typecheck_error(c_type))
      return false;

    if(told_type)
    {
      // Don't need to free type here on exit
      if(!coerce_literals(&ele, type, opt))
        return false;

      c_type = ast_type(ele); // May have changed due to literals
      ast_t* w_type = consume_type(type, TK_NONE);

      errorframe_t info = NULL;
      errorframe_t frame = NULL;
      if(w_type == NULL)
      {
        ast_error_frame(&frame, ele,
          "invalid specified array element type: %s",
          ast_print_type(type));
        errorframe_append(&frame, &info);
        errorframe_report(&frame, opt->check.errors);
        return false;
      }
      else if(!is_subtype(c_type, w_type, &info, opt))
      {
        ast_error_frame(&frame, ele,
          "array element not a subtype of specified array type");
        ast_error_frame(&frame, type_spec, "array type: %s",
          ast_print_type(type));
        ast_error_frame(&frame, c_type, "element type: %s",
          ast_print_type(c_type));
        errorframe_append(&frame, &info);
        errorframe_report(&frame, opt->check.errors);
        ast_free_unattached(w_type);
        return false;
      }
    }
    else
    {
      if(is_type_literal(c_type))
      {
        // At least one array element is literal, so whole array is
        ast_free_unattached(type);
        make_literal_type(ast);
        return true;
      }

      type = type_union(opt, type, c_type);
    }
  }

  if(!told_type)
  {
    ast_t* aliasable_type = type;
    type = alias(aliasable_type);
    ast_free_unattached(aliasable_type);
  }

  BUILD(ref, ast, NODE(TK_REFERENCE, ID("Array")));

  BUILD(qualify, ast,
    NODE(TK_QUALIFY,
      TREE(ref)
      NODE(TK_TYPEARGS, TREE(type))));

  BUILD(dot, ast, NODE(TK_DOT, TREE(qualify) ID("create")));

  ast_t* size_arg = ast_from_int(ast, size);
  BUILD(size_arg_seq, ast, NODE(TK_SEQ, TREE(size_arg)));
  ast_settype(size_arg, type_builtin(opt, ast, "USize"));
  ast_settype(size_arg_seq, type_builtin(opt, ast, "USize"));

  BUILD(call, ast,
    NODE(TK_CALL,
      TREE(dot)
      NODE(TK_POSITIONALARGS, TREE(size_arg_seq))
      NONE
      NONE));

  if(!refer_reference(opt, &ref) ||
    !refer_qualify(opt, qualify) ||
    !expr_typeref(opt, &qualify) ||
    !expr_dot(opt, &dot) ||
    !expr_call(opt, &call)
    )
    return false;

  ast_swap(ast, call);
  *astp = call;

  elements = ast_childidx(ast, 1);

  for(ast_t* ele = ast_child(elements); ele != NULL; ele = ast_sibling(ele))
  {
    BUILD(append_chain, ast, NODE(TK_CHAIN, TREE(*astp) ID("push")));
    BUILD(ele_seq, ast, NODE(TK_SEQ, TREE(ele)));

    BUILD(append, ast,
      NODE(TK_CALL,
        TREE(append_chain)
        NODE(TK_POSITIONALARGS, TREE(ele_seq))
        NONE
        NONE));

    ast_replace(astp, append);

    if(!expr_chain(opt, &append_chain) ||
      !expr_seq(opt, ele_seq) ||
      !expr_call(opt, &append)
      )
      return false;
  }

  ast_free_unattached(ast);
  return true;
}
