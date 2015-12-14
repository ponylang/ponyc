#include "casemethod.h"
#include "sugar.h"
#include "../ast/astbuild.h"
#include "../ast/id.h"
#include "../pkg/package.h"
#include <assert.h>


/* The following sugar handles case methods.

Case methods are the methods appearing in the source code appearing to be
overloaded. A match method is the set of case methods for a given method name
within a single entity, treated as a single overall method.

We remove case methods from an entity and replace them with notionally a single
function. In practice we actually create 2 functions, to allow us to make
parameter and capture names work sensibly. These 2 functions are:

1. The wrapper function.
Uses the parameter names specified in the case methods, which allows calling
with named arguments to work as expected.
All this function does is call the worker function.
For case behaviours this is a behaviour, for case functions it is a function.

2. The worker method.
This uses hygenic parameter names, which leaves the names used by the case
methods free to be used for captures.
Contains the match expression that selects which case body to use.

Example source code:

class C
  fun foo(0): U64 => 0
  fun foo(x: U64 where x < 3): U64 => 1
  fun foo(x: U64): U64 => 2

would be translated to:

class C
  fun foo(x: U64): U64 => // wrapper
    $1(x)

  fun $1($2: U64): U64 => // worker
    match $2
    | 0 => 0
    | let x: U64 where x < 3 => 1
    | let x: U64 => 2
    end

*/


// Make a skeleton wrapper method based on the given case method.
// Returns: created method, NULL on error.
static ast_t* make_match_wrapper(ast_t* case_method)
{
  assert(case_method != NULL);

  if(ast_id(case_method) == TK_FUN)
    fun_defaults(case_method);

  AST_GET_CHILDREN(case_method, cap, id, t_params, params, ret_type, question,
    body, docstring);

  if(ast_child(params) == NULL)
  {
    ast_error(params, "case method must have at least one parameter");
    return NULL;
  }

  ast_t* new_params = ast_from(params, TK_PARAMS);
  ast_t* param_list_end = NULL;

  for(ast_t* p = ast_child(params); p != NULL; p = ast_sibling(p))
  {
    // Set all parameter info to none now and fill it in later.
    BUILD(new_param, p, NODE(TK_PARAM, NONE NONE NONE));
    ast_list_append(new_params, &param_list_end, new_param);
  }

  ast_t* new_t_params = ast_from(params, TK_NONE);
  ast_t* t_param_list_end = NULL;

  for(ast_t* p = ast_child(t_params); p != NULL; p = ast_sibling(p))
  {
    // Set all type parameter info to none now and fill it in later.
    BUILD(new_t_param, p, NODE(TK_TYPEPARAM, NONE NONE NONE));
    ast_list_append(new_t_params, &t_param_list_end, new_t_param);
    ast_setid(new_t_params, TK_TYPEPARAMS);
  }

  if(ast_id(case_method) == TK_FUN)
  {
    // Function case.

    // TODO: For now always need a `None |` in the return type to allow for
    // match else clause. We won't need that once exhaustive matching is done.
    BUILD(wrapper, case_method,
      NODE(ast_id(case_method), AST_SCOPE
        TREE(cap)
        TREE(id)
        TREE(new_t_params)
        TREE(new_params)
        NODE(TK_NOMINAL, NONE ID("None") NONE NONE NONE)  // Return value.
        NONE    // Error.
        NODE(TK_SEQ)  // Body.
        NONE    // Doc string.
        NONE)); // Guard.

    return wrapper;
  }
  else
  {
    // Behaviour case.
    BUILD(wrapper, case_method,
      NODE(ast_id(case_method), AST_SCOPE
      NONE    // Capability.
      TREE(id)
      TREE(new_t_params)
      TREE(new_params)
      NONE    // Return value.
      NONE    // Error.
      NODE(TK_SEQ)  // Body.
      NONE    // Doc string.
      NONE)); // Guard.

    return wrapper;
  }
}


// Handle the given case method parameter, which does have a type specified.
// Combine and check the case parameter against the existing match parameter
// and generate the pattern element.
// Returns: true on success, false on error.
static bool param_with_type(ast_t* case_param, ast_t* match_param,
  ast_t* pattern)
{
  assert(case_param != NULL);
  assert(match_param != NULL);
  assert(pattern != NULL);

  AST_GET_CHILDREN(case_param, case_id, case_type, case_def_arg);
  assert(ast_id(case_type) != TK_NONE);

  if(ast_id(case_id) != TK_ID)
  {
    ast_error(case_id, "expected parameter name for case method. If this is "
      "meant to be a value to match on, do not specify parameter type");
    return false;
  }

  AST_GET_CHILDREN(match_param, match_id, match_type, match_def_arg);
  assert(ast_id(match_id) == TK_ID || ast_id(match_id) == TK_NONE);

  if(ast_id(match_id) == TK_NONE)
  {
    // This is the first case method to give this parameter, just use it as
    // given by this case.
    ast_replace(&match_id, case_id);
    ast_replace(&match_type, case_type);
    ast_replace(&match_def_arg, case_def_arg);
  }
  else
  {
    // Combine new case param with existing match param.
    if(ast_name(case_id) != ast_name(match_id))
    {
      ast_error(case_id, "parameter name differs between case methods");
      ast_error(match_id, "clashing name here");
      return false;
    }

    if(ast_id(case_def_arg) != TK_NONE)
    {
      // This case provides a default argument.
      if(ast_id(match_def_arg) != TK_NONE)
      {
        ast_error(case_def_arg,
          "multiple defaults provided for case method parameter");
        ast_error(match_def_arg, "another default defined here");
        return false;
      }

      // Move default arg to match parameter.
      ast_replace(&match_def_arg, case_def_arg);
    }

    // Union param types.
    REPLACE(&match_type,
      NODE(TK_UNIONTYPE,
        TREE(match_type)
        TREE(case_type)));
  }

  // Add parameter to match pattern.
  BUILD(pattern_elem, case_param,
    NODE(TK_SEQ,
      NODE(TK_LET, TREE(case_id) TREE(case_type))));

  ast_append(pattern, pattern_elem);
  return true;
}


// Handle the given case method parameter, which does not have a type
// specified.
// Check the case parameter and generate the pattern element.
// Returns: true on success, false on error.
static bool param_without_type(ast_t* case_param, ast_t* pattern)
{
  assert(case_param != NULL);
  assert(pattern != NULL);

  AST_GET_CHILDREN(case_param, value, type, def_arg);
  assert(ast_id(type) == TK_NONE);

  if(ast_id(def_arg) != TK_NONE)
  {
    ast_error(type,
      "cannot specify default argument for match value parameter");
    return false;
  }

  // Add value to match pattern. Pop it first to avoid pointless copy.
  ast_t* popped_value = ast_pop(case_param);

  if(ast_id(popped_value) == TK_DONTCARE)
  {
    // Value is just `don't care`.
    ast_append(pattern, popped_value);
  }
  else
  {
    // Value in an expression, need a containing sequence.
    BUILD(value_ast, value, NODE(TK_SEQ, TREE(popped_value)));
    ast_append(pattern, value_ast);
  }

  return true;
}


// Handle the given case method parameter list.
// Combine and check the case parameters against the existing match parameters
// and generate the match pattern to go in the worker method.
// Returns: match pattern, NULL on error.
static ast_t* process_params(ast_t* match_params, ast_t* case_params)
{
  assert(match_params != NULL);
  assert(case_params != NULL);

  bool ok = true;
  size_t count = 0;
  ast_t* pattern = ast_from(case_params, TK_TUPLE);
  ast_t* case_param = ast_child(case_params);
  ast_t* match_param = ast_child(match_params);

  while(case_param != NULL && match_param != NULL)
  {
    AST_GET_CHILDREN(case_param, id, type, def_arg);

    if(ast_id(type) != TK_NONE)
    {
      // We have a parameter.
      if(!param_with_type(case_param, match_param, pattern))
        ok = false;
    }
    else
    {
      // We have a value to match.
      if(!param_without_type(case_param, pattern))
        ok = false;
    }

    case_param = ast_sibling(case_param);
    match_param = ast_sibling(match_param);
    count++;
  }

  if(case_param != NULL || match_param != NULL)
  {
    ast_error(case_params, "differing number of parameters to case methods");
    ast_error(match_params, "clashing parameter count here");
    ok = false;
  }

  if(!ok)
  {
    ast_free(pattern);
    return NULL;
  }

  assert(count > 0);
  if(count == 1)
  {
    // Only one parameter, don't need a tuple pattern.
    ast_t* tmp = ast_pop(ast_child(pattern));
    ast_free(pattern);
    pattern = tmp;
  }

return pattern;
}


// Check parameters and build the all parameter structures for a complete set
// of case methods with the given name.
// Generate match operand, worker parameters and worker call arguments.
// Returns: match operand, NULL on failure.
static ast_t* build_params(ast_t* match_params, ast_t* worker_params,
  ast_t* call_args, const char* name, typecheck_t* t)
{
  assert(match_params != NULL);
  assert(worker_params != NULL);
  assert(call_args != NULL);
  assert(name != NULL);
  assert(t != NULL);

  ast_t* match_operand = ast_from(match_params, TK_TUPLE);
  size_t count = 0;
  bool ok = true;

  for(ast_t* p = ast_child(match_params); p != NULL; p = ast_sibling(p))
  {
    AST_GET_CHILDREN(p, id, type, def_arg);
    count++;

    if(ast_id(id) == TK_NONE)
    {
      assert(ast_id(type) == TK_NONE);
      ast_error(p,
        "name and type not specified for parameter " __zu " of case function %s",
        count, name);
      ok = false;
    }
    else
    {
      const char* worker_param_name = package_hygienic_id(t);

      // Add parameter to match operand.
      BUILD(element, p,
        NODE(TK_SEQ,
        NODE(TK_CONSUME,
        NONE
        NODE(TK_REFERENCE, ID(worker_param_name)))));

      ast_append(match_operand, element);

      // Add parameter to worker function.
      BUILD(param, p,
        NODE(TK_PARAM,
        ID(worker_param_name)
        TREE(type)
        NONE)); // Default argument.

      ast_append(worker_params, param);

      // Add argument to worker call.
      BUILD(arg, p,
        NODE(TK_SEQ,
        NODE(TK_CONSUME,
        NONE
        NODE(TK_REFERENCE, TREE(id)))));

      ast_append(call_args, arg);
    }
  }

  if(!ok)
  {
    ast_free(match_operand);
    return NULL;
  }

  assert(count > 0);
  if(count == 1)
  {
    // Only one parameter, don't need tuple in operand.
    ast_t* tmp = ast_pop(ast_child(match_operand));
    ast_free(match_operand);
    match_operand = tmp;
  }

  return match_operand;
}


// Handle the given case method type parameter.
// Combine and check the case type parameter against the existing match type
// parameter.
// Returns: true on success, false on error.
static bool process_t_param(ast_t* case_param, ast_t* match_param)
{
  assert(case_param != NULL);
  assert(match_param != NULL);

  AST_GET_CHILDREN(case_param, case_id, case_constraint, case_def_type);
  assert(ast_id(case_id) == TK_ID);

  AST_GET_CHILDREN(match_param, match_id, match_constraint, match_def_type);
  assert(ast_id(match_id) == TK_ID || ast_id(match_id) == TK_NONE);

  if(ast_id(match_id) == TK_NONE)
  {
    // This is the first case method to give this type parameter, just use it
    // as given by this case.
    ast_replace(&match_id, case_id);
    ast_replace(&match_constraint, case_constraint);
    ast_replace(&match_def_type, case_def_type);
  }
  else
  {
    // Combine new case param with existing match param.
    if(ast_name(case_id) != ast_name(match_id))
    {
      ast_error(case_id, "type parameter name differs between case methods");
      ast_error(match_id, "clashing name here");
      return false;
    }

    if(ast_id(case_def_type) != TK_NONE)
    {
      // This case provides a default argument.
      if(ast_id(match_def_type) != TK_NONE)
      {
        ast_error(case_def_type,
          "multiple defaults provided for case method type parameter");
        ast_error(match_def_type, "another default defined here");
        return false;
      }

      // Move default arg to match parameter.
      ast_replace(&match_def_type, case_def_type);
    }

    if(ast_id(case_constraint) != TK_NONE)
    {
      if(ast_id(match_constraint) == TK_NONE)
      {
        // Case has our first constraint, use it.
        ast_replace(&match_constraint, case_constraint);
      }
      else
      {
        // Case and existing match both have constraints, intersect them.
        REPLACE(&match_constraint,
          NODE(TK_ISECTTYPE,
            TREE(match_constraint)
            TREE(case_constraint)));
      }
    }
  }

  return true;
}


// Handle the given case method type parameter list.
// Combine and check the case type parameters against the existing match type
// parameters.
// Returns: true on success, false on error.
static bool process_t_params(ast_t* match_params, ast_t* case_params)
{
  assert(match_params != NULL);
  assert(case_params != NULL);

  bool ok = true;
  ast_t* case_param = ast_child(case_params);
  ast_t* match_param = ast_child(match_params);

  while(case_param != NULL && match_param != NULL)
  {
    if(!process_t_param(case_param, match_param))
      ok = false;

    case_param = ast_sibling(case_param);
    match_param = ast_sibling(match_param);
  }

  if(case_param != NULL || match_param != NULL)
  {
    ast_error(case_params,
      "differing number of type parameters to case methods");
    ast_error(match_params, "clashing type parameter count here");
    ok = false;
  }

  return ok;
}


// Check type parameters and build the all type parameter structures for a
// complete set of case methods with the given name.
// Generate type parameter list for worker call.
static void build_t_params(ast_t* match_t_params,
  ast_t* worker_t_params)
{
  assert(match_t_params != NULL);
  assert(worker_t_params != NULL);

  for(ast_t* p = ast_child(match_t_params); p != NULL; p = ast_sibling(p))
  {
    AST_GET_CHILDREN(p, id, constraint, def_type);
    assert(ast_id(id) == TK_ID);

    // Add type parameter name to worker call list.
    BUILD(type, p, NODE(TK_NOMINAL, NONE TREE(id) NONE NONE NONE));
    ast_append(worker_t_params, type);
    ast_setid(worker_t_params, TK_TYPEARGS);
  }
}


// Add the given case method into the given match method wrapper and check the
// are compatible.
// Returns: match case for worker method or NULL on error.
static ast_t* add_case_method(ast_t* match_method, ast_t* case_method)
{
  assert(match_method != NULL);
  assert(case_method != NULL);

  // We need default capabality and return value if not provided explicitly.
  if(ast_id(case_method) == TK_FUN)
    fun_defaults(case_method);

  AST_GET_CHILDREN(match_method, match_cap, match_id, match_t_params,
    match_params, match_ret_type, match_question);

  AST_GET_CHILDREN(case_method, case_cap, case_id, case_t_params, case_params,
    case_ret_type, case_question, case_body, case_docstring, case_guard);

  bool ok = true;

  if(ast_id(case_method) != ast_id(match_method))
  {
    ast_error(case_method,
      "cannot mix fun and be cases in a single match method");
    ast_error(match_method, "clashing method here");
    ok = false;
  }

  if(ast_id(case_method) == TK_FUN)
  {
    if(ast_id(case_cap) != ast_id(match_cap))
    {
      ast_error(case_cap, "differing receiver capabilities on case methods");
      ast_error(match_cap, "clashing capability here");
      ok = false;
    }

    if(ast_id(match_ret_type) == TK_NONE)
    {
      // Use case method return type.
      ast_replace(&match_ret_type, case_ret_type);
    }
    else
    {
      // Union this case method's return type with the existing match one.
      REPLACE(&match_ret_type,
        NODE(TK_UNIONTYPE,
        TREE(match_ret_type)
        TREE(case_ret_type)));
    }
  }

  if(ast_id(case_question) == TK_QUESTION)
    // If any case throws the match does too.
    ast_setid(match_question, TK_QUESTION);

  if(!process_t_params(match_t_params, case_t_params))
    ok = false;

  ast_t* pattern = process_params(match_params, case_params);

  if(!ok || pattern == NULL)
  {
    ast_free(pattern);
    return NULL;
  }

  // Extract case body and guard condition (if any) to avoid copying.
  ast_t* body = ast_from(case_body, TK_NONE);
  ast_swap(case_body, body);
  ast_t* guard = ast_from(case_guard, TK_NONE);
  ast_swap(case_guard, guard);

  // Make match case.
  BUILD(match_case, pattern,
    NODE(TK_CASE, AST_SCOPE
      TREE(pattern)
      TREE(case_guard)
      TREE(case_body)));

  return match_case;
}


// Handle a set of case methods, starting at the given method.
// Generate wrapper and worker methods and append them to the given list.
// Returns: true on success, false on failure.
static bool sugar_case_method(ast_t* first_case_method, ast_t* members,
  const char* name, typecheck_t* t)
{
  assert(first_case_method != NULL);
  assert(members != NULL);
  assert(name != NULL);
  assert(t != NULL);

  ast_t* wrapper = make_match_wrapper(first_case_method);

  if(wrapper == NULL)
    return false;

  ast_t* match_cases = ast_from(first_case_method, TK_CASES);
  ast_scope(match_cases);

  // Process all methods with the same name.
  // We need to remove processed methods. However, removing nodes from a list
  // we're iterating over gets rather complex. Instead we mark nodes and remove
  // them all in one sweep later.
  for(ast_t* p = first_case_method; p != NULL; p = ast_sibling(p))
  {
    const char* p_name = ast_name(ast_childidx(p, 1));

    if(p_name == name)
    {
      // This method is in the case set.
      ast_t* case_ast = add_case_method(wrapper, p);

      if(case_ast == NULL)
      {
        ast_free(wrapper);
        ast_free(match_cases);
        return false;
      }

      ast_append(match_cases, case_ast);
      ast_setid(p, TK_NONE);  // Mark for removal.
    }
  }

  // Check params and build match operand, worker parameters and worker call
  // arguments.
  ast_t* match_params = ast_childidx(wrapper, 3);
  ast_t* worker_params = ast_from(match_params, TK_PARAMS);
  ast_t* call_args = ast_from(match_params, TK_POSITIONALARGS);
  ast_t* match_operand = build_params(match_params, worker_params, call_args,
    name, t);

  if(match_operand == NULL)
  {
    ast_free(wrapper);
    ast_free(match_cases);
    ast_free(worker_params);
    ast_free(call_args);
    return false;
  }

  // Complete wrapper function and add to containing entity.
  const char* worker_name = package_hygienic_id(t);
  ast_t* wrapper_body = ast_childidx(wrapper, 6);
  ast_t* wrapper_call;
  ast_t* call_t_args = ast_from(wrapper, TK_NONE);
  assert(ast_id(wrapper_body) == TK_SEQ);
  assert(ast_childcount(wrapper_body) == 0);

  build_t_params(ast_childidx(wrapper, 2), call_t_args);

  if(ast_id(call_t_args) == TK_NONE)
  {
    // No type args needed.
    ast_free(call_t_args);
    BUILD(tmp, wrapper_body,
      NODE(TK_CALL,
        TREE(call_args)
        NONE
        NODE(TK_REFERENCE, ID(worker_name))));

    wrapper_call = tmp;
  }
  else
  {
    // Type args needed on call.
    BUILD(tmp, wrapper_body,
      NODE(TK_CALL,
        TREE(call_args)
        NONE
        NODE(TK_QUALIFY,
          NODE(TK_REFERENCE, ID(worker_name))
          TREE(call_t_args))));

    wrapper_call = tmp;
  }

  ast_append(wrapper_body, wrapper_call);
  ast_append(members, wrapper);

  // Make worker function and add to containing entity.
  AST_GET_CHILDREN(wrapper, cap, wrapper_id, t_params, wrapper_params,
    ret_type, error);

  if(ast_id(wrapper) == TK_BE)
    cap = ast_from(cap, TK_REF);

  BUILD(worker, wrapper,
    NODE(TK_FUN, AST_SCOPE
      TREE(cap)
      ID(worker_name)
      TREE(t_params)
      TREE(worker_params)
      TREE(ret_type)
      TREE(error)
      NODE(TK_SEQ,
        NODE(TK_MATCH, AST_SCOPE
          NODE(TK_SEQ, TREE(match_operand))
          TREE(match_cases)
          NONE)) // Else clause.
      NONE    // Doc string.
      NONE)); // Guard.

  ast_append(members, worker);
  return true;
}


// Sugar any case methods in the given entity.
ast_result_t sugar_case_methods(typecheck_t* t, ast_t* entity)
{
  assert(entity != NULL);

  ast_result_t result = AST_OK;
  ast_t* members = ast_childidx(entity, 4);
  bool cases_found = false;

  // Check each method to see if its name is repeated, which indicates a case
  // method.
  for(ast_t* p = ast_child(members); p != NULL; p = ast_sibling(p))
  {
    if(ast_id(p) == TK_FUN || ast_id(p) == TK_BE)
    {
      const char* p_name = ast_name(ast_childidx(p, 1));

      // Check if any subsequent methods share p's name.
      for(ast_t* q = ast_sibling(p); q != NULL; q = ast_sibling(q))
      {
        if(ast_id(q) == TK_FUN || ast_id(q) == TK_BE)
        {
          const char* q_name = ast_name(ast_childidx(q, 1));

          if(q_name == p_name)
          {
            // p's name is repeated, it's a case method, get a match method.
            if(!sugar_case_method(p, members, p_name, t))
              result = AST_ERROR;

            cases_found = true;
            break;
          }
        }
      }
    }
  }

  if(cases_found)
  {
    // Remove nodes marked during processing.
    ast_t* filtered_members = ast_from(members, TK_MEMBERS);
    ast_t* list = NULL;
    ast_t* member;

    while((member = ast_pop(members)) != NULL)
    {
      if(ast_id(member) == TK_NONE) // Marked for removal.
        ast_free(member);
      else  // Put back in filtered list.
        ast_list_append(filtered_members, &list, member);
    }

    ast_replace(&members, filtered_members);
  }

  return result;
}
