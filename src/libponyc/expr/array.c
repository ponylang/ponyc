#include "array.h"
#include "literal.h"
#include "../ast/astbuild.h"
#include "../pkg/package.h"
#include "../pass/names.h"
#include "../pass/expr.h"
#include "../type/alias.h"
#include "../type/assemble.h"
#include "../type/reify.h"
#include "../type/subtype.h"
#include "../type/lookup.h"
#include "../type/typealias.h"
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

  switch(ast_id(ast))
  {
    case TK_TUPLETYPE:
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      ast_t* new_ast = ast_from(ast, ast_id(ast));

      for(ast_t* c = ast_child(ast); c != NULL; c = ast_sibling(c))
        ast_append(new_ast, strip_this_arrow(opt, c));

      return new_ast;
    }

    default:
      break;
  }

  return ast;
}

static ast_t* detect_apply_element_type(pass_opt_t* opt, ast_t* ast, ast_t* def)
{
  // The interface must have an apply method for us to find it.
  ast_t* apply = ast_get(def, stringtab(opt->strtab, "apply"), NULL);
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
  if(ast_name(ast_childidx(param_type, 1)) != stringtab(opt->strtab, "USize"))
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
  ast_t* values = ast_get(def, stringtab(opt->strtab, "values"), NULL);
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
    (ast_name(ast_childidx(ret_type, 1)) != stringtab(opt->strtab, "Iterator")) ||
    (ast_childcount(ast_childidx(ret_type, 2)) != 1))
    return NULL;

  // Based on the return type we try to figure out the element type.
  ast_t* elem_type = ast_child(ast_childidx(ret_type, 2));

  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* typeargs = ast_childidx(ast, 2);
  if(ast_id(typeparams) == TK_TYPEPARAMS)
    elem_type = reify(elem_type, typeparams, typeargs, opt, true);

  // Strip `this->` viewpoint arrows, including any that were distributed
  // into a union/intersection/tuple when a type alias was expanded.
  return strip_this_arrow(opt, elem_type);
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
      if(stringtab(opt->strtab, "Array") == ast_name(name))
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

    case TK_TYPEALIASREF:
    {
      // Don't free the unfolded AST: the TK_NOMINAL case stores pointers
      // into it (ast_child(typeargs) for Array element types) that the
      // caller uses.
      ast_t* unfolded = typealias_unfold(ast);

      if(unfolded != NULL)
        find_possible_element_types(opt, unfolded, list);

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

      if(stringtab(opt->strtab, "Iterator") == ast_name(name))
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

    case TK_TYPEALIASREF:
    {
      // Don't free: same reason as find_possible_element_types above.
      ast_t* unfolded = typealias_unfold(ast);

      if(unfolded != NULL)
        find_possible_iterator_element_types(opt, unfolded, list);

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
      (ast_name(ast_sibling(ast)) == stringtab(opt->strtab, "values")))
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
      ast_t* cursor_type = consume_type(astlist_data(cursor), TK_NONE, false,
        opt);

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
    {
      ast_free_unattached(type);
      return false;
    }

    if(told_type)
    {
      // Don't need to free type here on exit
      if(!coerce_literals(&ele, type, opt))
        return false;

      c_type = ast_type(ele); // May have changed due to literals
      ast_t* w_type = consume_type(type, TK_NONE, false, opt);

      errorframe_t info = NULL;
      errorframe_t frame = NULL;
      if(w_type == NULL)
      {
        ast_error_frame(&frame, ele,
          "invalid specified array element type: %s",
          ast_print_type(type, opt->strtab));
        errorframe_append(&frame, &info);
        errorframe_report(&frame, opt->check.errors);
        return false;
      }
      else if(!is_subtype(c_type, w_type, &info, opt))
      {
        ast_error_frame(&frame, ele,
          "array element not a subtype of specified array type");
        ast_error_frame(&frame, type_spec, "array type: %s",
          ast_print_type(type, opt->strtab));
        ast_error_frame(&frame, c_type, "element type: %s",
          ast_print_type(c_type, opt->strtab));
        errorframe_append(&frame, &info);
        errorframe_report(&frame, opt->check.errors);
        ast_free_unattached(w_type);
        return false;
      }

      ast_free_unattached(w_type);
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

      ast_t* prev_type = type;
      type = type_union(opt, prev_type, c_type);

      // type_union may return prev_type, c_type, or a freshly-built tree.
      // Free any input it did not return as-is: ast_free_unattached is a
      // no-op on aliases (still parented) and on NULL, so no ownership
      // tracking is needed.
      if(type != prev_type)
        ast_free_unattached(prev_type);
      if(type != c_type)
        ast_free_unattached(c_type);
    }
  }

  if(!told_type)
  {
    ast_t* aliasable_type = type;
    type = alias(aliasable_type, opt);
    ast_free_unattached(aliasable_type);
  }

  // Desugar the literal to a flat sequence that fills the array through a temp
  // local:
  //   (let $array = Array[T].create(N); $array.push(e1); ...; $array)
  // rather than the left-nested `Array[T].create(N).>push(e1).>push(e2)...`
  // chain this used to build. The chain deepened by one level per element, and
  // re-attaching the growing expression each iteration made building it O(N^2):
  // TREE() duplicates an already-parented node and ast_replace() frees one, so
  // both touched the whole accumulated tree every time. The flat form touches
  // only fixed-size nodes per element, so the build is O(N). The sequence is
  // left unprocessed and run through ast_passes_subtree, which sets up the temp
  // local and resolves its references through the normal front-end passes
  // (sugar through expr).
  BUILD(array_create, ast,
    NODE(TK_CALL,
      NODE(TK_DOT,
        NODE(TK_QUALIFY,
          NODE(TK_REFERENCE, ID("Array"))
          NODE(TK_TYPEARGS, TREE(type)))
        ID("create"))
      NODE(TK_POSITIONALARGS, NODE(TK_SEQ, INT(size)))
      NONE
      NONE));

  if(size == 0)
  {
    // No elements to push: leave the array as a bare constructor call. An
    // embedded field must be assigned a constructor. is_expr_constructor
    // (expr/operator.c) looks through a sequence to its last expression, so the
    // size>0 form above is rejected for an embed field not because it is a
    // sequence but because it ends in the temp-local reference; keeping the bare
    // create for an empty literal is what lets `embed field = []` stay valid.
    ast_replace(astp, array_create);
  }
  else
  {
    const char* tmp_name = package_hygienic_id(&opt->check, opt);

    // No AST_SCOPE on the sequence: an expression-position sequence is a
    // rawseq, exactly as Pony represents a parenthesised `(let x = ...; x)`.
    // The temp local is defined in the enclosing scope; its hygienic name
    // rules out any collision. A scope here would make the node a scoped seq,
    // which the tree-check `expr` group rejects in expression position.
    BUILD(seq, ast,
      NODE(TK_SEQ,
        NODE(TK_ASSIGN,
          NODE(TK_LET, NICE_ID(tmp_name, "array literal") NONE)
          TREE(array_create))));

    // Track the last sibling so each push is appended in O(1) instead of
    // walking to the end of the sequence's child list.
    ast_t* last = ast_child(seq); // the TK_ASSIGN
    for(ast_t* ele = ast_pop(elements); ele != NULL; ele = ast_pop(elements))
    {
      BUILD(push, ast,
        NODE(TK_CALL,
          NODE(TK_DOT, NODE(TK_REFERENCE, ID(tmp_name)) ID("push"))
          NODE(TK_POSITIONALARGS, NODE(TK_SEQ, TREE(ele)))
          NONE
          NONE));
      last = ast_add_sibling(last, push);
    }

    BUILD(array_ref, ast, NODE(TK_REFERENCE, ID(tmp_name)));
    ast_add_sibling(last, array_ref);

    ast_replace(astp, seq);
  }

  // Run the front-end passes over the spliced-in nodes. The already-processed
  // elements carry the expr pass flag, so each pass skips them; only the new
  // structural nodes (the let, references, push calls and sequence) are
  // processed.
  return ast_passes_subtree(astp, opt, PASS_EXPR);
}
