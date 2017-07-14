#include "sugar.h"
#include "casemethod.h"
#include "../ast/astbuild.h"
#include "../ast/id.h"
#include "../ast/printbuf.h"
#include "../pass/syntax.h"
#include "../pkg/ifdef.h"
#include "../pkg/package.h"
#include "../type/alias.h"
#include "../type/assemble.h"
#include "../type/sanitise.h"
#include "../type/subtype.h"
#include "../ast/stringtab.h"
#include "../ast/token.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <stdio.h>
#include <string.h>


static ast_t* make_create(ast_t* ast)
{
  pony_assert(ast != NULL);

  // Default constructors on classes can be iso, on actors they must be tag
  token_id cap = TK_NONE;

  switch(ast_id(ast))
  {
    case TK_STRUCT:
    case TK_CLASS:
      cap = TK_ISO;
      break;

    default: {}
  }

  BUILD(create, ast,
    NODE(TK_NEW, AST_SCOPE
      NODE(cap)
      ID("create")  // name
      NONE          // typeparams
      NONE          // params
      NONE          // return type
      NONE          // error
      NODE(TK_SEQ, NODE(TK_TRUE))
      NONE
      NONE
      ));

  return create;
}


bool has_member(ast_t* members, const char* name)
{
  name = stringtab(name);
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    ast_t* id;

    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      case TK_EMBED:
        id = ast_child(member);
        break;

      default:
        id = ast_childidx(member, 1);
        break;
    }

    if(ast_name(id) == name)
      return true;

    member = ast_sibling(member);
  }

  return false;
}


static void add_default_constructor(ast_t* ast)
{
  pony_assert(ast != NULL);
  ast_t* members = ast_childidx(ast, 4);

  // If we have no constructors and no "create" member, add a "create"
  // constructor.
  if(has_member(members, "create"))
    return;

  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_NEW:
        return;

      default: {}
    }

    member = ast_sibling(member);
  }

  ast_append(members, make_create(ast));
}


static ast_result_t sugar_module(pass_opt_t* opt, ast_t* ast)
{
  ast_t* docstring = ast_child(ast);

  ast_t* package = ast_parent(ast);
  pony_assert(ast_id(package) == TK_PACKAGE);

  if(strcmp(package_name(package), "$0") != 0)
  {
    // Every module not in builtin has an implicit use builtin command.
    // Since builtin is always the first package processed it is $0.
    BUILD(builtin, ast,
      NODE(TK_USE,
      NONE
      STRING(stringtab("builtin"))
      NONE));

    ast_add(ast, builtin);
  }

  if((docstring == NULL) || (ast_id(docstring) != TK_STRING))
    return AST_OK;

  ast_t* package_docstring = ast_childlast(package);

  if(ast_id(package_docstring) == TK_STRING)
  {
    ast_error(opt->check.errors, docstring,
      "the package already has a docstring");
    ast_error_continue(opt->check.errors, package_docstring,
      "the existing docstring is here");
    return AST_ERROR;
  }

  ast_append(package, docstring);
  ast_remove(docstring);
  return AST_OK;
}


static ast_result_t sugar_entity(pass_opt_t* opt, ast_t* ast, bool add_create,
  token_id def_def_cap)
{
  AST_GET_CHILDREN(ast, id, typeparams, defcap, traits, members);

  if(add_create)
    add_default_constructor(ast);

  if(ast_id(defcap) == TK_NONE)
    ast_setid(defcap, def_def_cap);

  // Build a reverse sequence of all field initialisers.
  BUILD(init_seq, members, NODE(TK_SEQ));
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FLET:
      case TK_FVAR:
      case TK_EMBED:
      {
        AST_GET_CHILDREN(member, f_id, f_type, f_init);

        if(ast_id(f_init) != TK_NONE)
        {
          // Replace the initialiser with TK_NONE.
          ast_swap(f_init, ast_from(f_init, TK_NONE));

          // id = init
          BUILD(init, member,
            NODE(TK_ASSIGN,
            TREE(f_init)
            NODE(TK_REFERENCE, TREE(f_id))));

          ast_add(init_seq, init);
        }
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  // Add field initialisers to all constructors.
  if(ast_child(init_seq) != NULL)
  {
    member = ast_child(members);

    while(member != NULL)
    {
      switch(ast_id(member))
      {
        case TK_NEW:
        {
          AST_GET_CHILDREN(member, n_cap, n_id, n_typeparam, n_params,
            n_result, n_partial, n_body);

          pony_assert(ast_id(n_body) == TK_SEQ);

          ast_t* init = ast_child(init_seq);

          while(init != NULL)
          {
            ast_add(n_body, init);
            init = ast_sibling(init);
          }
          break;
        }

        default: {}
      }

      member = ast_sibling(member);
    }
  }

  ast_free_unattached(init_seq);

  return sugar_case_methods(opt, ast);
}


static ast_result_t sugar_typeparam(ast_t* ast)
{
  AST_GET_CHILDREN(ast, id, constraint);

  if(ast_id(constraint) == TK_NONE)
  {
    REPLACE(&constraint,
      NODE(TK_NOMINAL,
        NONE
        TREE(id)
        NONE
        NONE
        NONE));
  }

  return AST_OK;
}


static void sugar_docstring(ast_t* ast)
{
  pony_assert(ast != NULL);

  AST_GET_CHILDREN(ast, cap, id, type_params, params, return_type,
    error, body, docstring);

  if(ast_id(docstring) == TK_NONE)
  {
    ast_t* first = ast_child(body);

    // First expression in body is a docstring if it is a string literal and
    // there are any other expressions in the body sequence
    if((first != NULL) &&
      (ast_id(first) == TK_STRING) &&
      (ast_sibling(first) != NULL))
    {
      ast_pop(body);
      ast_replace(&docstring, first);
    }
  }
}


// Check the parameters are proper parameters and not
// something nasty let in by the case method value parsing.
static ast_result_t check_params(pass_opt_t* opt, ast_t* params)
{
  pony_assert(params != NULL);
  ast_result_t result = AST_OK;

  // Check each parameter.
  for(ast_t* p = ast_child(params); p != NULL; p = ast_sibling(p))
  {
    if(ast_id(p) == TK_ELLIPSIS)
      continue;

    AST_GET_CHILDREN(p, id, type, def_arg);

    if(ast_id(id) != TK_ID)
    {
      ast_error(opt->check.errors, p, "expected parameter name");
      result = AST_ERROR;
    }
    else if(!is_name_internal_test(ast_name(id)) && !check_id_param(opt, id))
    {
      result = AST_ERROR;
    }

    if(ast_id(type) == TK_NONE)
    {
      ast_error(opt->check.errors, type, "expected parameter type");
      result = AST_ERROR;
    }
  }

  return result;
}


// Check for leftover stuff from case methods.
static ast_result_t check_method(pass_opt_t* opt, ast_t* method)
{
  pony_assert(method != NULL);

  ast_result_t result = AST_OK;
  ast_t* params = ast_childidx(method, 3);
  result = check_params(opt, params);

  // Also check the guard expression doesn't exist.
  ast_t* guard = ast_childidx(method, 8);
  pony_assert(guard != NULL);

  if(ast_id(guard) != TK_NONE)
  {
    ast_error(opt->check.errors, guard,
      "only case methods can have guard conditions");
    result = AST_ERROR;
  }

  return result;
}


static ast_result_t sugar_new(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result);

  // Return type default to ref^ for classes, val^ for primitives, and
  // tag^ for actors.
  if(ast_id(result) == TK_NONE)
  {
    token_id tcap = ast_id(cap);

    if(tcap == TK_NONE)
    {
      switch(ast_id(opt->check.frame->type))
      {
        case TK_PRIMITIVE: tcap = TK_VAL; break;
        case TK_ACTOR: tcap = TK_TAG; break;
        default: tcap = TK_REF; break;
      }

      ast_setid(cap, tcap);
    }

    ast_replace(&result, type_for_this(opt, ast, tcap, TK_EPHEMERAL));
  }

  sugar_docstring(ast);
  return check_method(opt, ast);
}


static ast_result_t sugar_be(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error, body);
  ast_setid(cap, TK_TAG);

  if(ast_id(result) == TK_NONE)
  {
    // Return type is None.
    ast_t* type = type_sugar(ast, NULL, "None");
    ast_replace(&result, type);
  }

  sugar_docstring(ast);
  return check_method(opt, ast);
}


void fun_defaults(ast_t* ast)
{
  pony_assert(ast != NULL);
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error, body,
    docstring);

  // If the receiver cap is not specified, set it to box.
  if(ast_id(cap) == TK_NONE)
    ast_setid(cap, TK_BOX);

  // If the return value is not specified, set it to None.
  if(ast_id(result) == TK_NONE)
  {
    ast_t* type = type_sugar(ast, NULL, "None");
    ast_replace(&result, type);
  }

  // If the return type is None, add a None at the end of the body, unless it
  // already ends with an error or return statement
  if(is_none(result) && (ast_id(body) != TK_NONE))
  {
    ast_t* last_cmd = ast_childlast(body);

    if(ast_id(last_cmd) != TK_ERROR && ast_id(last_cmd) != TK_RETURN)
    {
      BUILD(ref, body, NODE(TK_REFERENCE, ID("None")));
      ast_append(body, ref);
    }
  }
}


static ast_result_t sugar_fun(pass_opt_t* opt, ast_t* ast)
{
  fun_defaults(ast);
  sugar_docstring(ast);
  return check_method(opt, ast);
}


// If the given tree is a TK_NONE expand it to a source None
static void expand_none(ast_t* ast, bool is_scope)
{
  if(ast_id(ast) != TK_NONE)
    return;

  if(is_scope)
    ast_scope(ast);

  ast_setid(ast, TK_SEQ);
  BUILD(ref, ast, NODE(TK_REFERENCE, ID("None")));
  ast_add(ast, ref);
}


static ast_result_t sugar_return(pass_opt_t* opt, ast_t* ast)
{
  ast_t* return_value = ast_child(ast);

  if((ast_id(ast) == TK_RETURN) && (ast_id(opt->check.frame->method) == TK_NEW))
  {
    pony_assert(ast_id(return_value) == TK_NONE);
    ast_setid(return_value, TK_THIS);
  } else {
    expand_none(return_value, false);
  }

  return AST_OK;
}


static ast_result_t sugar_else(ast_t* ast, size_t index)
{
  ast_t* else_clause = ast_childidx(ast, index);
  expand_none(else_clause, true);
  return AST_OK;
}


static ast_result_t sugar_try(ast_t* ast)
{
  AST_GET_CHILDREN(ast, ignore, else_clause, then_clause);

  if(ast_id(else_clause) == TK_NONE && ast_id(then_clause) != TK_NONE)
    // Then without else means we don't require a throwable in the try block
    ast_setid(ast, TK_TRY_NO_CHECK);

  expand_none(else_clause, true);
  expand_none(then_clause, true);

  return AST_OK;
}


static ast_result_t sugar_for(pass_opt_t* opt, ast_t** astp)
{
  AST_EXTRACT_CHILDREN(*astp, for_idseq, for_iter, for_body, for_else);
  ast_t* annotation = ast_consumeannotation(*astp);

  expand_none(for_else, true);
  const char* iter_name = package_hygienic_id(&opt->check);

  BUILD(try_next, for_iter,
    NODE(TK_TRY_NO_CHECK,
      NODE(TK_SEQ, AST_SCOPE
        NODE(TK_CALL,
          NONE
          NONE
          NODE(TK_QUESTION)
          NODE(TK_DOT, NODE(TK_REFERENCE, ID(iter_name)) ID("next"))))
      NODE(TK_SEQ, AST_SCOPE
        NODE(TK_BREAK, NONE))
      NONE));

  sugar_try(try_next);

  REPLACE(astp,
    NODE(TK_SEQ,
      NODE(TK_ASSIGN,
        TREE(for_iter)
        NODE(TK_LET, NICE_ID(iter_name, "for loop iterator") NONE))
      NODE(TK_WHILE, AST_SCOPE
        ANNOTATE(annotation)
        NODE(TK_SEQ,
          NODE_ERROR_AT(TK_CALL, for_iter,
            NONE
            NONE
            NONE
            NODE(TK_DOT, NODE(TK_REFERENCE, ID(iter_name)) ID("has_next"))))
        NODE(TK_SEQ, AST_SCOPE
          NODE_ERROR_AT(TK_ASSIGN, for_idseq,
            TREE(try_next)
            TREE(for_idseq))
          TREE(for_body))
        TREE(for_else))));

  return AST_OK;
}


static void build_with_dispose(ast_t* dispose_clause, ast_t* idseq)
{
  pony_assert(dispose_clause != NULL);
  pony_assert(idseq != NULL);

  if(ast_id(idseq) == TK_LET)
  {
    // Just a single variable
    ast_t* id = ast_child(idseq);
    pony_assert(id != NULL);

    // Don't call dispose() on don't cares
    if(is_name_dontcare(ast_name(id)))
      return;

    BUILD(dispose, idseq,
      NODE(TK_CALL,
        NONE
        NONE
        NONE
        NODE(TK_DOT, NODE(TK_REFERENCE, TREE(id)) ID("dispose"))));

    ast_add(dispose_clause, dispose);
    return;
  }

  // We have a list of variables
  pony_assert(ast_id(idseq) == TK_TUPLE);

  for(ast_t* p = ast_child(idseq); p != NULL; p = ast_sibling(p))
    build_with_dispose(dispose_clause, p);
}


static ast_result_t sugar_with(pass_opt_t* opt, ast_t** astp)
{
  AST_EXTRACT_CHILDREN(*astp, withexpr, body, else_clause);
  ast_t* main_annotation = ast_consumeannotation(*astp);
  ast_t* else_annotation = ast_consumeannotation(else_clause);
  token_id try_token;

  if(ast_id(else_clause) == TK_NONE)
    try_token = TK_TRY_NO_CHECK;
  else
    try_token = TK_TRY;

  expand_none(else_clause, false);

  // First build a skeleton try block without the "with" variables
  BUILD(replace, *astp,
    NODE(TK_SEQ,
      NODE(try_token,
        ANNOTATE(main_annotation)
        NODE(TK_SEQ, AST_SCOPE
          TREE(body))
        NODE(TK_SEQ, AST_SCOPE
          ANNOTATE(else_annotation)
          TREE(else_clause))
        NODE(TK_SEQ, AST_SCOPE))));

  ast_t* tryexpr = ast_child(replace);
  AST_GET_CHILDREN(tryexpr, try_body, try_else, try_then);

  // Add the "with" variables from each with element
  for(ast_t* p = ast_child(withexpr); p != NULL; p = ast_sibling(p))
  {
    pony_assert(ast_id(p) == TK_SEQ);
    AST_GET_CHILDREN(p, idseq, init);
    const char* init_name = package_hygienic_id(&opt->check);

    BUILD(assign, idseq,
      NODE(TK_ASSIGN,
        TREE(init)
        NODE(TK_LET, ID(init_name) NONE)));

    BUILD(local, idseq,
      NODE(TK_ASSIGN,
        NODE(TK_REFERENCE, ID(init_name))
        TREE(idseq)));

    ast_add(replace, assign);
    ast_add(try_body, local);
    ast_add(try_else, local);
    build_with_dispose(try_then, idseq);
    ast_add(try_then, local);
  }

  ast_replace(astp, replace);
  return AST_OK;
}


// Find all captures in the given pattern.
static bool sugar_match_capture(pass_opt_t* opt, ast_t* pattern)
{
  switch(ast_id(pattern))
  {
    case TK_VAR:
      ast_error(opt->check.errors, pattern,
        "match captures may not be declared with `var`, use `let`");
      return false;

    case TK_LET:
    {
      AST_GET_CHILDREN(pattern, id, capture_type);

      if(ast_id(capture_type) == TK_NONE)
      {
        ast_error(opt->check.errors, pattern,
          "capture types cannot be inferred, please specify type of %s",
          ast_name(id));

        return false;
      }

      // Disallow capturing tuples.
      if(ast_id(capture_type) == TK_TUPLETYPE)
      {
        ast_error(opt->check.errors, capture_type,
          "can't capture a tuple, change this into a tuple of capture "
          "expressions");

        return false;
      }

      // Change this to a capture.
      ast_setid(pattern, TK_MATCH_CAPTURE);

      return true;
    }

    case TK_TUPLE:
    {
      // Check all tuple elements.
      bool r = true;

      for(ast_t* p = ast_child(pattern); p != NULL; p = ast_sibling(p))
      {
        if(!sugar_match_capture(opt, p))
          r = false;
      }

      return r;
    }

    case TK_SEQ:
      // Captures in a sequence must be the only element.
      if(ast_childcount(pattern) != 1)
        return true;

      return sugar_match_capture(opt, ast_child(pattern));

    default:
      // Anything else isn't a capture.
      return true;
  }
}


static ast_result_t sugar_case(pass_opt_t* opt, ast_t* ast)
{
  ast_result_t r = AST_OK;

  AST_GET_CHILDREN(ast, pattern, guard, body);

  if(!sugar_match_capture(opt, pattern))
    r = AST_ERROR;

  if(ast_id(body) != TK_NONE)
    return r;

  // We have no body, take a copy of the next case with a body
  ast_t* next = ast;
  ast_t* next_body = body;

  while(ast_id(next_body) == TK_NONE)
  {
    next = ast_sibling(next);
    pony_assert(next != NULL);
    pony_assert(ast_id(next) == TK_CASE);
    next_body = ast_childidx(next, 2);
  }

  ast_replace(&body, next_body);
  return r;
}


static ast_result_t sugar_update(ast_t** astp)
{
  ast_t* ast = *astp;
  pony_assert(ast_id(ast) == TK_ASSIGN);

  AST_GET_CHILDREN(ast, value, call);

  if(ast_id(call) != TK_CALL)
    return AST_OK;

  // We are of the form:  x(y) = z
  // Replace us with:     x.update(y where value = z)
  AST_EXTRACT_CHILDREN(call, positional, named, question, expr);

  // If there are no named arguments yet, named will be a TK_NONE.
  ast_setid(named, TK_NAMEDARGS);

  // Build a new namedarg.
  BUILD(namedarg, ast,
    NODE(TK_UPDATEARG,
      ID("value")
      NODE(TK_SEQ, TREE(value))));

  // Append the named arg to our existing list.
  ast_append(named, namedarg);

  // Replace with the update call.
  REPLACE(astp,
    NODE(TK_CALL,
      TREE(positional)
      TREE(named)
      TREE(question)
      NODE(TK_DOT, TREE(expr) ID("update"))));

  return AST_OK;
}


static ast_result_t sugar_as(pass_opt_t* opt, ast_t** astp)
{
  (void)opt;
  ast_t* ast = *astp;
  pony_assert(ast_id(ast) == TK_AS);
  AST_GET_CHILDREN(ast, expr, type);

  if(ast_id(type) == TK_TUPLETYPE)
  {
    BUILD(new_type, type, NODE(TK_TUPLETYPE));

    for(ast_t* p = ast_child(type); p != NULL; p = ast_sibling(p))
    {
      if(ast_id(p) == TK_NOMINAL &&
        is_name_dontcare(ast_name(ast_childidx(p, 1))))
      {
        BUILD(dontcare, new_type, NODE(TK_DONTCARETYPE));
        ast_append(new_type, dontcare);
      }
      else
        ast_append(new_type, p);
    }

    REPLACE(astp,
      NODE(TK_AS, TREE(expr) TREE(new_type)));
  }

  return AST_OK;
}


static ast_result_t sugar_binop(ast_t** astp, const char* fn_name)
{
  AST_GET_CHILDREN(*astp, left, right);

  ast_t* positional = ast_from(right, TK_POSITIONALARGS);

  if(ast_id(right) == TK_TUPLE)
  {
    ast_t* value = ast_child(right);

    while(value != NULL)
    {
      BUILD(arg, right, NODE(TK_SEQ, TREE(value)));
      ast_append(positional, arg);
      value = ast_sibling(value);
    }
  } else {
    BUILD(arg, right, NODE(TK_SEQ, TREE(right)));
    ast_add(positional, arg);
  }

  REPLACE(astp,
    NODE(TK_CALL,
      TREE(positional)
      NONE
      NONE
      NODE(TK_DOT, TREE(left) ID(fn_name))
      ));

  return AST_OK;
}


static ast_result_t sugar_unop(ast_t** astp, const char* fn_name)
{
  AST_GET_CHILDREN(*astp, expr);

  REPLACE(astp,
    NODE(TK_CALL,
      NONE
      NONE
      NONE
      NODE(TK_DOT, TREE(expr) ID(fn_name))
      ));

  return AST_OK;
}


static ast_result_t sugar_ffi(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, id, typeargs, args, named_args);

  const char* name = ast_name(id);
  size_t len = ast_name_len(id);

  // Check for \0 in ffi name (it may be a string literal)
  if(memchr(name, '\0', len) != NULL)
  {
    ast_error(opt->check.errors, ast,
      "FFI function names cannot include null characters");
    return AST_ERROR;
  }

  // Prefix '@' to the name
  char* new_name = (char*)ponyint_pool_alloc_size(len + 2);
  new_name[0] = '@';
  memcpy(new_name + 1, name, len);
  new_name[len + 1] = '\0';

  ast_t* new_id = ast_from_string(id, stringtab_consume(new_name, len + 2));
  ast_replace(&id, new_id);

  if(ast_id(ast) == TK_FFIDECL)
    return check_params(opt, args);

  return AST_OK;
}


static ast_result_t sugar_ifdef(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(opt != NULL);
  pony_assert(ast != NULL);

  AST_GET_CHILDREN(ast, cond, then_block, else_block, else_cond);

  // Combine parent ifdef condition with ours.
  ast_t* parent_ifdef_cond = opt->check.frame->ifdef_cond;

  if(parent_ifdef_cond != NULL)
  {
    // We have a parent ifdef, combine its condition with ours.
    pony_assert(ast_id(ast_parent(parent_ifdef_cond)) == TK_IFDEF);

    REPLACE(&else_cond,
      NODE(TK_AND,
        TREE(parent_ifdef_cond)
        NODE(TK_NOT, TREE(cond))));

    REPLACE(&cond,
      NODE(TK_AND,
        TREE(parent_ifdef_cond)
        TREE(cond)));
  }
  else
  {
    // Make else condition for our children to use.
    REPLACE(&else_cond, NODE(TK_NOT, TREE(cond)));
  }

  // Normalise condition so and, or and not nodes aren't sugared to function
  // calls.
  if(!ifdef_cond_normalise(&cond, opt))
  {
    ast_error(opt->check.errors, ast, "ifdef condition will never be true");
    return AST_ERROR;
  }

  if(!ifdef_cond_normalise(&else_cond, opt))
  {
    ast_error(opt->check.errors, ast, "ifdef condition is always true");
    return AST_ERROR;
  }

  return sugar_else(ast, 2);
}


static ast_result_t sugar_use(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast != NULL);

  // Normalise condition so and, or and not nodes aren't sugared to function
  // calls.
  ast_t* guard = ast_childidx(ast, 2);

  if(!ifdef_cond_normalise(&guard, opt))
  {
    ast_error(opt->check.errors, ast, "use guard condition will never be true");
    return AST_ERROR;
  }

  return AST_OK;
}


static ast_result_t sugar_semi(pass_opt_t* options, ast_t** astp)
{
  ast_t* ast = *astp;
  pony_assert(ast_id(ast) == TK_SEMI);

  // Semis are pointless, discard them
  pony_assert(ast_child(ast) == NULL);
  *astp = ast_sibling(ast);
  ast_remove(ast);

  // Since we've effectively replaced ast with its successor we need to process
  // that too
  return pass_sugar(astp, options);
}


static ast_result_t sugar_lambdatype(pass_opt_t* opt, ast_t** astp)
{
  pony_assert(astp != NULL);
  ast_t* ast = *astp;
  pony_assert(ast != NULL);

  AST_EXTRACT_CHILDREN(ast, apply_cap, apply_name, apply_t_params, params,
    ret_type, error, interface_cap, ephemeral);

  bool bare = ast_id(ast) == TK_BARELAMBDATYPE;

  if(bare)
  {
    ast_setid(apply_cap, TK_AT);
    ast_setid(interface_cap, TK_VAL);
  }

  const char* i_name = package_hygienic_id(&opt->check);

  ast_t* interface_t_params;
  ast_t* t_args;
  collect_type_params(ast, NULL, &t_args);

  // We will replace {..} with $0
  REPLACE(astp,
    NODE(TK_NOMINAL,
      NONE  // Package
      ID(i_name)
      TREE(t_args)
      TREE(interface_cap)
      TREE(ephemeral)));

  ast = *astp;

  // Fetch the interface type parameters after we replace the ast, so that if
  // we are an interface type parameter, we get ourselves as the constraint.
  collect_type_params(ast, &interface_t_params, NULL);

  printbuf_t* buf = printbuf_new();

  // Include the receiver capability or the bareness if appropriate.
  if(ast_id(apply_cap) == TK_AT)
    printbuf(buf, "@{(");
  else if(ast_id(apply_cap) != TK_NONE)
    printbuf(buf, "{%s(", ast_print_type(apply_cap));
  else
    printbuf(buf, "{(");

  // Convert parameter type list to a normal parameter list.
  int p_no = 1;
  for(ast_t* p = ast_child(params); p != NULL; p = ast_sibling(p))
  {
    if(p_no > 1)
      printbuf(buf, ", ");

    printbuf(buf, "%s", ast_print_type(p));

    char name[12];
    snprintf(name, sizeof(name), "p%d", p_no);

    REPLACE(&p,
      NODE(TK_PARAM,
        ID(name)
        TREE(p)
        NONE));

    p_no++;
  }

  printbuf(buf, ")");

  if(ast_id(ret_type) != TK_NONE)
    printbuf(buf, ": %s", ast_print_type(ret_type));

  if(ast_id(error) != TK_NONE)
    printbuf(buf, " ?");

  printbuf(buf, "}");

  const char* fn_name = "apply";

  if(ast_id(apply_name) == TK_ID)
    fn_name = ast_name(apply_name);

  // Attach the nice name to the original lambda.
  ast_setdata(ast_childidx(ast, 1), (void*)stringtab(buf->m));

  // Create a new anonymous type.
  BUILD(def, ast,
    NODE(TK_INTERFACE, AST_SCOPE
      NICE_ID(i_name, buf->m)
      TREE(interface_t_params)
      NONE  // Cap
      NONE  // Provides
      NODE(TK_MEMBERS,
        NODE(TK_FUN, AST_SCOPE
          TREE(apply_cap)
          ID(fn_name)
          TREE(apply_t_params)
          TREE(params)
          TREE(ret_type)
          TREE(error)
          NONE  // Body
          NONE  // Doc string
          NONE))// Guard
      NONE    // @
      NONE)); // Doc string

  printbuf_free(buf);

  if(bare)
  {
    BUILD(bare_annotation, def,
      NODE(TK_ANNOTATION,
        ID("ponyint_bare")));

    // Record the syntax pass as done to avoid the error about internal
    // annotations.
    ast_pass_record(bare_annotation, PASS_SYNTAX);
    ast_setannotation(def, bare_annotation);
  }

  // Add new type to current module and bring it up to date with passes.
  ast_t* module = ast_nearest(ast, TK_MODULE);
  ast_append(module, def);

  if(!ast_passes_type(&def, opt, opt->program_pass))
    return AST_FATAL;

  // Sugar the call.
  if(!ast_passes_subtree(astp, opt, PASS_SUGAR))
    return AST_FATAL;

  return AST_OK;
}


static ast_result_t sugar_barelambda(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;

  pony_assert(ast != NULL);

  AST_GET_CHILDREN(ast, receiver_cap, name, t_params, params, captures,
    ret_type, raises, body, reference_cap);

  ast_setid(receiver_cap, TK_AT);
  ast_setid(reference_cap, TK_VAL);

  return AST_OK;
}


ast_t* expand_location(ast_t* location)
{
  pony_assert(location != NULL);

  const char* file_name = ast_source(location)->file;

  if(file_name == NULL)
    file_name = "";

  // Find name of containing method.
  const char* method_name = "";
  for(ast_t* method = location; method != NULL; method = ast_parent(method))
  {
    token_id variety = ast_id(method);

    if(variety == TK_FUN || variety == TK_BE || variety == TK_NEW)
    {
      method_name = ast_name(ast_childidx(method, 1));
      break;
    }
  }

  // Create an object literal.
  BUILD(ast, location,
    NODE(TK_OBJECT, DATA("__loc")
      NONE  // Capability
      NONE  // Provides
      NODE(TK_MEMBERS,
        NODE(TK_FUN, AST_SCOPE
          NODE(TK_TAG) ID("file") NONE NONE
          NODE(TK_NOMINAL, NONE ID("String") NONE NONE NONE)
          NONE
          NODE(TK_SEQ, STRING(file_name))
          NONE NONE)
        NODE(TK_FUN, AST_SCOPE
          NODE(TK_TAG) ID("method") NONE NONE
          NODE(TK_NOMINAL, NONE ID("String") NONE NONE NONE)
          NONE
          NODE(TK_SEQ, STRING(method_name))
          NONE NONE)
        NODE(TK_FUN, AST_SCOPE
          NODE(TK_TAG) ID("line") NONE NONE
          NODE(TK_NOMINAL, NONE ID("USize") NONE NONE NONE)
          NONE
          NODE(TK_SEQ, INT(ast_line(location)))
          NONE NONE)
        NODE(TK_FUN, AST_SCOPE
          NODE(TK_TAG) ID("pos") NONE NONE
          NODE(TK_NOMINAL, NONE ID("USize") NONE NONE NONE)
          NONE
          NODE(TK_SEQ, INT(ast_pos(location)))
          NONE NONE))));

  return ast;
}


static ast_result_t sugar_location(pass_opt_t* opt, ast_t** astp)
{
  pony_assert(astp != NULL);
  ast_t* ast = *astp;
  pony_assert(ast != NULL);

  if(ast_id(ast_parent(ast)) == TK_PARAM)
    // Location is a default argument, do not expand yet.
    return AST_OK;

  ast_t* location = expand_location(ast);
  ast_replace(astp, location);

  // Sugar the expanded object.
  if(!ast_passes_subtree(astp, opt, PASS_SUGAR))
    return AST_FATAL;

  return AST_OK;
}


ast_result_t pass_sugar(ast_t** astp, pass_opt_t* options)
{
  ast_t* ast = *astp;
  pony_assert(ast != NULL);

  switch(ast_id(ast))
  {
    case TK_MODULE:           return sugar_module(options, ast);
    case TK_PRIMITIVE:        return sugar_entity(options, ast, true, TK_VAL);
    case TK_STRUCT:           return sugar_entity(options, ast, true, TK_REF);
    case TK_CLASS:            return sugar_entity(options, ast, true, TK_REF);
    case TK_ACTOR:            return sugar_entity(options, ast, true, TK_TAG);
    case TK_TRAIT:
    case TK_INTERFACE:        return sugar_entity(options, ast, false, TK_REF);
    case TK_TYPEPARAM:        return sugar_typeparam(ast);
    case TK_NEW:              return sugar_new(options, ast);
    case TK_BE:               return sugar_be(options, ast);
    case TK_FUN:              return sugar_fun(options, ast);
    case TK_RETURN:           return sugar_return(options, ast);
    case TK_IF:
    case TK_WHILE:
    case TK_REPEAT:           return sugar_else(ast, 2);
    case TK_IFTYPE_SET:       return sugar_else(ast, 1);
    case TK_TRY:              return sugar_try(ast);
    case TK_FOR:              return sugar_for(options, astp);
    case TK_WITH:             return sugar_with(options, astp);
    case TK_CASE:             return sugar_case(options, ast);
    case TK_ASSIGN:           return sugar_update(astp);
    case TK_AS:               return sugar_as(options, astp);
    case TK_PLUS:             return sugar_binop(astp, "add");
    case TK_MINUS:            return sugar_binop(astp, "sub");
    case TK_MULTIPLY:         return sugar_binop(astp, "mul");
    case TK_DIVIDE:           return sugar_binop(astp, "div");
    case TK_MOD:              return sugar_binop(astp, "mod");
    case TK_PLUS_TILDE:       return sugar_binop(astp, "add_unsafe");
    case TK_MINUS_TILDE:      return sugar_binop(astp, "sub_unsafe");
    case TK_MULTIPLY_TILDE:   return sugar_binop(astp, "mul_unsafe");
    case TK_DIVIDE_TILDE:     return sugar_binop(astp, "div_unsafe");
    case TK_MOD_TILDE:        return sugar_binop(astp, "mod_unsafe");
    case TK_LSHIFT:           return sugar_binop(astp, "shl");
    case TK_RSHIFT:           return sugar_binop(astp, "shr");
    case TK_LSHIFT_TILDE:     return sugar_binop(astp, "shl_unsafe");
    case TK_RSHIFT_TILDE:     return sugar_binop(astp, "shr_unsafe");
    case TK_AND:              return sugar_binop(astp, "op_and");
    case TK_OR:               return sugar_binop(astp, "op_or");
    case TK_XOR:              return sugar_binop(astp, "op_xor");
    case TK_EQ:               return sugar_binop(astp, "eq");
    case TK_NE:               return sugar_binop(astp, "ne");
    case TK_LT:               return sugar_binop(astp, "lt");
    case TK_LE:               return sugar_binop(astp, "le");
    case TK_GE:               return sugar_binop(astp, "ge");
    case TK_GT:               return sugar_binop(astp, "gt");
    case TK_EQ_TILDE:         return sugar_binop(astp, "eq_unsafe");
    case TK_NE_TILDE:         return sugar_binop(astp, "ne_unsafe");
    case TK_LT_TILDE:         return sugar_binop(astp, "lt_unsafe");
    case TK_LE_TILDE:         return sugar_binop(astp, "le_unsafe");
    case TK_GE_TILDE:         return sugar_binop(astp, "ge_unsafe");
    case TK_GT_TILDE:         return sugar_binop(astp, "gt_unsafe");
    case TK_UNARY_MINUS:      return sugar_unop(astp, "neg");
    case TK_UNARY_MINUS_TILDE:return sugar_unop(astp, "neg_unsafe");
    case TK_NOT:              return sugar_unop(astp, "op_not");
    case TK_FFIDECL:
    case TK_FFICALL:          return sugar_ffi(options, ast);
    case TK_IFDEF:            return sugar_ifdef(options, ast);
    case TK_USE:              return sugar_use(options, ast);
    case TK_SEMI:             return sugar_semi(options, astp);
    case TK_LAMBDATYPE:
    case TK_BARELAMBDATYPE:   return sugar_lambdatype(options, astp);
    case TK_BARELAMBDA:       return sugar_barelambda(options, ast);
    case TK_LOCATION:         return sugar_location(options, astp);
    default:                  return AST_OK;
  }
}
