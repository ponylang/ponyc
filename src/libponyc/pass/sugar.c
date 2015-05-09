#include "sugar.h"
#include "../ast/astbuild.h"
#include "../pkg/package.h"
#include "../type/alias.h"
#include "../type/assemble.h"
#include "../type/subtype.h"
#include "../ast/stringtab.h"
#include "../ast/token.h"
#include <string.h>
#include <assert.h>


static ast_t* make_create(ast_t* ast)
{
  BUILD(create, ast,
    NODE(TK_NEW, AST_SCOPE
      NONE          // cap
      ID("create")  // name
      NONE          // typeparams
      NONE          // params
      NONE          // return type
      NONE          // error
      NODE(TK_SEQ, NODE(TK_TRUE))
      NONE
      ));

  return create;
}


static bool has_member(ast_t* members, const char* name)
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


static void add_default_constructor(ast_t* members)
{
  // If we have no uninitialised fields and no "create" member, add a "create"
  // constructor
  if(has_member(members, "create"))
    return;

  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      {
        ast_t* init = ast_childidx(member, 2);

        if(ast_id(init) == TK_NONE)
          return;

        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  ast_append(members, make_create(members));
}


static ast_t* make_nominal(ast_t* id, ast_t* typeargs)
{
  BUILD_NO_DEBUG(type, id,
    NODE(TK_NOMINAL,
      NONE
      TREE(id)
      TREE(typeargs)
      NONE
      NONE));

  return type;
}


static void add_comparable(ast_t* id, ast_t* typeparams, ast_t* members)
{
  BUILD_NO_DEBUG(typeargs, typeparams, NODE(TK_TYPEARGS));

  ast_t* typeparam = ast_child(typeparams);

  while(typeparam != NULL)
  {
    ast_t* p_id = ast_child(typeparam);
    ast_t* type = make_nominal(p_id, ast_from(id, TK_NONE));
    ast_append(typeargs, type);

    typeparam = ast_sibling(typeparam);
  }

  if(ast_child(typeargs) == NULL)
    ast_setid(typeargs, TK_NONE);

  ast_t* type = make_nominal(id, typeargs);

  if(!has_member(members, "eq"))
  {
    BUILD_NO_DEBUG(eq, members,
      NODE(TK_FUN, AST_SCOPE
        NODE(TK_BOX)
        ID("eq")
        NONE
        NODE(TK_PARAMS,
          NODE(TK_PARAM,
            ID("that")
            TREE(type)
            NONE))
        NODE(TK_NOMINAL,
          NONE
          ID("Bool")
          NONE
          NONE
          NONE)
        NONE
        NODE(TK_SEQ,
          NODE(TK_IS,
            NODE(TK_THIS)
            NODE(TK_REFERENCE, ID("that"))))
        NONE));

    ast_append(members, eq);
  }

  if(!has_member(members, "ne"))
  {
    BUILD_NO_DEBUG(eq, members,
      NODE(TK_FUN, AST_SCOPE
        NODE(TK_BOX)
        ID("ne")
        NONE
        NODE(TK_PARAMS,
          NODE(TK_PARAM,
            ID("that")
            TREE(type)
            NONE))
        NODE(TK_NOMINAL,
          NONE
          ID("Bool")
          NONE
          NONE
          NONE)
        NONE
        NODE(TK_SEQ,
          NODE(TK_ISNT,
            NODE(TK_THIS)
            NODE(TK_REFERENCE, ID("that"))))
        NONE));

    ast_append(members, eq);
  }

  ast_free_unattached(type);
}


static ast_result_t sugar_module(ast_t* ast)
{
  ast_t* docstring = ast_child(ast);

  if((docstring == NULL) || (ast_id(docstring) != TK_STRING))
    return AST_OK;

  ast_t* package = ast_parent(ast);
  ast_t* package_docstring = ast_childlast(package);

  if(ast_id(package_docstring) == TK_STRING)
  {
    ast_error(docstring, "the package already has a docstring");
    ast_error(package_docstring, "the existing docstring is here");
    return AST_ERROR;
  }

  ast_append(package, docstring);
  ast_remove(docstring);
  return AST_OK;
}


static ast_result_t sugar_member(ast_t* ast, bool add_create, bool add_eq,
  token_id def_def_cap)
{
  AST_GET_CHILDREN(ast, id, typeparams, defcap, traits, members);

  if(add_create)
    add_default_constructor(members);

  if(ast_id(defcap) == TK_NONE)
    ast_setid(defcap, def_def_cap);

  if(add_eq)
    add_comparable(id, typeparams, members);

  return AST_OK;
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
  assert(ast != NULL);

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


static ast_result_t sugar_new(typecheck_t* t, ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result);

  // Return type default to ref^ for classes, val^ for primitives, and
  // tag^ for actors.
  if(ast_id(result) == TK_NONE)
  {
    token_id tcap = ast_id(cap);

    if(tcap == TK_NONE)
    {
      switch(ast_id(t->frame->type))
      {
        case TK_PRIMITIVE: tcap = TK_VAL; break;
        case TK_ACTOR: tcap = TK_TAG; break;
        default: tcap = TK_REF; break;
      }

      ast_setid(cap, tcap);
    }

    ast_replace(&result, type_for_this(t, ast, tcap, TK_EPHEMERAL));
  }

  sugar_docstring(ast);
  return AST_OK;
}


static ast_result_t sugar_be(typecheck_t* t, ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error, body);

  if(ast_id(result) == TK_NONE)
  {
    // Return type is This tag
    ast_replace(&result, type_for_this(t, ast, TK_TAG, TK_NONE));
  }

  sugar_docstring(ast);
  return AST_OK;
}


static ast_result_t sugar_fun(ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error, body);

  // If the receiver cap is not specified, set it to box.
  if(ast_id(cap) == TK_NONE)
    ast_setid(cap, TK_BOX);

  // If the return value is not specified, set it to None
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
      BUILD_NO_DEBUG(ref, body, NODE(TK_REFERENCE, ID("None")));
      ast_append(body, ref);
    }
  }

  sugar_docstring(ast);
  return AST_OK;
}


// If the given tree is a TK_NONE expand it to a source None
static void expand_none(ast_t* ast)
{
  if(ast_id(ast) != TK_NONE)
    return;

  ast_setid(ast, TK_SEQ);
  ast_scope(ast);
  BUILD_NO_DEBUG(ref, ast, NODE(TK_REFERENCE, ID("None")));
  ast_add(ast, ref);
}


static ast_result_t sugar_return(ast_t* ast)
{
  ast_t* return_value = ast_child(ast);
  expand_none(return_value);
  return AST_OK;
}


static ast_result_t sugar_else(ast_t* ast)
{
  ast_t* else_clause = ast_childidx(ast, 2);
  expand_none(else_clause);
  return AST_OK;
}


static ast_result_t sugar_try(ast_t* ast)
{
  AST_GET_CHILDREN(ast, ignore, else_clause, then_clause);

  if(ast_id(else_clause) == TK_NONE && ast_id(then_clause) != TK_NONE)
    // Then without else means we don't require a throwable in the try block
    ast_setid(ast, TK_TRY_NO_CHECK);

  expand_none(else_clause);
  expand_none(then_clause);

  return AST_OK;
}


static ast_result_t sugar_for(typecheck_t* t, ast_t** astp)
{
  AST_EXTRACT_CHILDREN(*astp, for_idseq, for_iter, for_body, for_else);

  expand_none(for_else);
  const char* iter_name = package_hygienic_id(t);

  BUILD(try_next, for_iter,
    NODE(TK_TRY_NO_CHECK,
      NODE(TK_SEQ, AST_SCOPE
        NODE(TK_CALL,
          NONE
          NONE
          NODE(TK_DOT, NODE(TK_REFERENCE, ID(iter_name)) ID("next"))))
      NODE(TK_SEQ, AST_SCOPE
        NODE(TK_CONTINUE, NONE))
      NONE));

  sugar_try(try_next);

  REPLACE(astp,
    NODE(TK_SEQ,
      NODE(TK_ASSIGN, AST_NODEBUG
        TREE(for_iter)
        NODE(TK_LET, ID(iter_name) NONE))
      NODE(TK_WHILE, AST_SCOPE
        NODE(TK_SEQ,
          NODE_ERROR_AT(TK_CALL, for_iter,
            NONE
            NONE
            NODE(TK_DOT, NODE(TK_REFERENCE, ID(iter_name)) ID("has_next"))))
        NODE(TK_SEQ, AST_SCOPE
          NODE_ERROR_AT(TK_ASSIGN, for_idseq, AST_NODEBUG
            TREE(try_next)
            TREE(for_idseq))
          TREE(for_body))
        TREE(for_else))));

  return AST_OK;
}


static void build_with_dispose(ast_t* dispose_clause, ast_t* idseq)
{
  assert(dispose_clause != NULL);
  assert(idseq != NULL);

  if(ast_id(idseq) == TK_LET)
  {
    // Just a single variable
    ast_t* id = ast_child(idseq);
    assert(id != NULL);

    // Don't call dispose() on don't cares
    if(ast_id(id) == TK_DONTCARE)
      return;

    assert(ast_id(id) == TK_ID);
    BUILD(dispose, idseq,
      NODE(TK_CALL,
        NONE NONE
        NODE(TK_DOT, NODE(TK_REFERENCE, TREE(id)) ID("dispose"))));

    ast_add(dispose_clause, dispose);
    return;
  }

  // We have a list of variables
  assert(ast_id(idseq) == TK_TUPLE);

  for(ast_t* p = ast_child(idseq); p != NULL; p = ast_sibling(p))
    build_with_dispose(dispose_clause, p);
}


static ast_result_t sugar_with(typecheck_t* t, ast_t** astp)
{
  AST_EXTRACT_CHILDREN(*astp, withexpr, body, else_clause);
  token_id try_token;

  if(ast_id(else_clause) == TK_NONE)
    try_token = TK_TRY_NO_CHECK;
  else
    try_token = TK_TRY;

  expand_none(else_clause);

  // First build a skeleton try block without the "with" variables
  BUILD(replace, *astp,
    NODE(TK_SEQ,
      NODE(try_token, AST_SCOPE
        NODE(TK_SEQ, AST_SCOPE
          TREE(body))
        NODE(TK_SEQ, AST_SCOPE
          TREE(else_clause))
        NODE(TK_SEQ, AST_SCOPE))));

  ast_t* tryexpr = ast_child(replace);
  AST_GET_CHILDREN(tryexpr, try_body, try_else, try_then);

  // Add the "with" variables from each with element
  for(ast_t* p = ast_child(withexpr); p != NULL; p = ast_sibling(p))
  {
    assert(ast_id(p) == TK_SEQ);
    AST_GET_CHILDREN(p, idseq, init);
    const char* init_name = package_hygienic_id(t);

    BUILD(assign, idseq,
      NODE(TK_ASSIGN, AST_NODEBUG
        TREE(init)
        NODE(TK_LET, ID(init_name) NONE)));

    BUILD(local, idseq,
      NODE(TK_ASSIGN, AST_NODEBUG
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


static ast_result_t sugar_case(ast_t* ast)
{
  ast_t* body = ast_childidx(ast, 2);

  if(ast_id(body) != TK_NONE)
    return AST_OK;

  // We have no body, take a copy of the next case with a body
  ast_t* next = ast;
  ast_t* next_body = body;

  while(ast_id(next_body) == TK_NONE)
  {
    next = ast_sibling(next);
    assert(next != NULL);
    assert(ast_id(next) == TK_CASE);
    next_body = ast_childidx(next, 2);
  }

  ast_replace(&body, next_body);
  return AST_OK;
}


static ast_result_t sugar_update(ast_t** astp)
{
  ast_t* ast = *astp;
  assert(ast_id(ast) == TK_ASSIGN);

  AST_GET_CHILDREN(ast, value, call);

  if(ast_id(call) != TK_CALL)
    return AST_OK;

  // We are of the form:  x(y) = z
  // Replace us with:     x.update(y where value = z)
  AST_EXTRACT_CHILDREN(call, positional, named, expr);

  // If there are no named arguments yet, named will be a TK_NONE.
  ast_setid(named, TK_NAMEDARGS);

  // Build a new namedarg.
  BUILD_NO_DEBUG(namedarg, ast,
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
      NODE(TK_DOT, TREE(expr) ID("update"))));

  return AST_OK;
}


static ast_result_t sugar_object(pass_opt_t* opt, ast_t** astp)
{
  typecheck_t* t = &opt->check;
  ast_t* ast = *astp;
  AST_GET_CHILDREN(ast, provides, members);
  ast_t* c_id = ast_from_string(ast, package_hygienic_id(t));

  // Create a new anonymous type.
  BUILD(def, ast,
    NODE(TK_CLASS, AST_SCOPE
      TREE(c_id)
      NONE
      NONE
      TREE(provides)
      NODE(TK_MEMBERS)
      NONE
      NONE));

  // We will have a create method in the type.
  BUILD_NO_DEBUG(create, members,
    NODE(TK_NEW, AST_SCOPE
      NONE
      ID("create")
      NONE
      NODE(TK_PARAMS)
      NONE
      NONE
      NODE(TK_SEQ)
      NONE));

  // We will replace object..end with $0.create(...)
  BUILD(call, ast,
    NODE(TK_CALL,
      NODE(TK_POSITIONALARGS)
      NONE
      NODE(TK_DOT, NODE(TK_REFERENCE, TREE(c_id)) ID("create"))));

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
      {
        AST_GET_CHILDREN(member, id, type, init);
        ast_t* p_id = ast_from_string(id, package_hygienic_id(t));

        // The field is: var/let id: type
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

        // The body of create contains: id = consume $0
        BUILD_NO_DEBUG(assign, init,
          NODE(TK_ASSIGN,
            NODE(TK_CONSUME, NODE(TK_NONE) NODE(TK_REFERENCE, TREE(p_id)))
            NODE(TK_REFERENCE, TREE(id))));

        ast_append(class_members, field);
        ast_append(create_params, param);
        ast_append(create_body, assign);
        ast_append(call_args, init);

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
    // Change the type from a class to a primitive.
    ast_setid(def, TK_PRIMITIVE);

    // End the constructor with None, since it has no parameters.
    BUILD_NO_DEBUG(none, ast, NODE(TK_REFERENCE, ID("None")));
    ast_append(create_body, none);
  }

  if(has_behaviours)
  {
    // Change the type to an actor.
    ast_setid(def, TK_ACTOR);
  }

  // Add the create function at the end.
  ast_append(class_members, create);

  // Add the new type to the current module.
  ast_t* module = ast_nearest(ast, TK_MODULE);
  ast_append(module, def);

  // Replace object..end with $0.create(...)
  ast_replace(astp, call);

  // Sugar the call.
  return ast_visit(astp, pass_sugar, NULL, opt);
}

static void add_as_type(typecheck_t* t, ast_t* type, ast_t* pattern,
  ast_t* body)
{
  assert(type != NULL);

  switch(ast_id(type))
  {
    case TK_TUPLETYPE:
    {
      BUILD(tuple_pattern, pattern, NODE(TK_SEQ, NODE(TK_TUPLE)));
      ast_append(pattern, tuple_pattern);
      ast_t* pattern_child = ast_child(tuple_pattern);

      BUILD(tuple_body, body, NODE(TK_SEQ, NODE(TK_TUPLE)));
      ast_t* body_child = ast_child(tuple_body);

      for(ast_t* p = ast_child(type); p != NULL; p = ast_sibling(p))
        add_as_type(t, p, pattern_child, body_child);

      if(ast_childcount(body_child) == 1)
      {
        // Only one child, not actually a tuple
        ast_t* t = ast_pop(body_child);
        ast_free(tuple_body);
        tuple_body = t;
      }

      ast_append(body, tuple_body);
      break;
    }

    case TK_DONTCARE:
      ast_append(pattern, type);
      break;

    default:
    {
      const char* name = package_hygienic_id(t);
      ast_t* a_type = alias(type);

      BUILD(pattern_elem, pattern,
        NODE(TK_SEQ,
          NODE(TK_LET, ID(name) TREE(a_type))));

      BUILD(body_elem, body,
        NODE(TK_SEQ,
          NODE(TK_CONSUME, NODE(TK_BORROWED) NODE(TK_REFERENCE, ID(name)))));

      ast_append(pattern, pattern_elem);
      ast_append(body, body_elem);
      break;
    }
  }
}


static ast_result_t sugar_as(pass_opt_t* opt, ast_t** astp)
{
  typecheck_t* t = &opt->check;
  ast_t* ast = *astp;
  AST_GET_CHILDREN(ast, expr, type);

  ast_t* pattern_root = ast_from(type, TK_LEX_ERROR);
  ast_t* body_root = ast_from(type, TK_LEX_ERROR);
  add_as_type(t, type, pattern_root, body_root);

  ast_t* body = ast_pop(body_root);
  ast_free(body_root);

  if(body == NULL)
  {
    // No body implies all types are "don't care"
    ast_error(ast, "Cannot treat value as \"don't care\"");
    ast_free(pattern_root);
    return AST_ERROR;
  }

  // Don't need top sequence in pattern
  assert(ast_id(ast_child(pattern_root)) == TK_SEQ);
  ast_t* pattern = ast_pop(ast_child(pattern_root));
  ast_free(pattern_root);

  REPLACE(astp,
    NODE(TK_MATCH, AST_SCOPE
      NODE(TK_SEQ, TREE(expr))
      NODE(TK_CASES, AST_SCOPE
        NODE(TK_CASE, AST_SCOPE
          TREE(pattern)
          NONE
          TREE(body)))
      NODE(TK_SEQ, AST_SCOPE NODE(TK_ERROR, NONE))));

  return ast_visit(astp, pass_sugar, NULL, opt);
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
      NODE(TK_DOT, TREE(expr) ID(fn_name))
      ));

  return AST_OK;
}


static ast_result_t sugar_ffi(ast_t* ast)
{
  AST_GET_CHILDREN(ast, id, typeargs, args, named_args);

  // Prefix '@' to the name.
  const char* name = ast_name(id);
  size_t len = strlen(name) + 1;

  VLA(char, new_name, len + 1);
  new_name[0] = '@';
  memcpy(new_name + 1, name, len);

  ast_t* new_id = ast_from_string(id, new_name);
  ast_replace(&id, new_id);

  return AST_OK;
}


static ast_result_t sugar_semi(ast_t** astp)
{
  ast_t* ast = *astp;
  assert(ast_id(ast) == TK_SEMI);

  // Semis are pointless, discard them
  assert(ast_child(ast) == NULL);
  *astp = ast_sibling(ast);
  ast_remove(ast);

  return AST_OK;
}


static ast_result_t sugar_let(typecheck_t* t, ast_t* ast)
{
  ast_t* id = ast_child(ast);

  if(ast_id(id) == TK_DONTCARE)
  {
    // Replace "_" with "$1" in with and for variable lists
    ast_setid(id, TK_ID);
    ast_set_name(id, package_hygienic_id(t));
  }

  return AST_OK;
}


ast_result_t pass_sugar(ast_t** astp, pass_opt_t* options)
{
  typecheck_t* t = &options->check;
  ast_t* ast = *astp;
  assert(ast != NULL);

  switch(ast_id(ast))
  {
    case TK_MODULE:     return sugar_module(ast);
    case TK_PRIMITIVE:  return sugar_member(ast, true, true, TK_VAL);
    case TK_CLASS:      return sugar_member(ast, true, false, TK_REF);
    case TK_ACTOR:      return sugar_member(ast, true, false, TK_TAG);
    case TK_TRAIT:
    case TK_INTERFACE:  return sugar_member(ast, false, false, TK_REF);
    case TK_TYPEPARAM:  return sugar_typeparam(ast);
    case TK_NEW:        return sugar_new(t, ast);
    case TK_BE:         return sugar_be(t, ast);
    case TK_FUN:        return sugar_fun(ast);
    case TK_RETURN:
    case TK_BREAK:      return sugar_return(ast);
    case TK_IF:
    case TK_MATCH:
    case TK_WHILE:
    case TK_REPEAT:     return sugar_else(ast);
    case TK_TRY:        return sugar_try(ast);
    case TK_FOR:        return sugar_for(t, astp);
    case TK_WITH:       return sugar_with(t, astp);
    case TK_CASE:       return sugar_case(ast);
    case TK_ASSIGN:     return sugar_update(astp);
    case TK_OBJECT:     return sugar_object(options, astp);
    case TK_AS:         return sugar_as(options, astp);
    case TK_PLUS:       return sugar_binop(astp, "add");
    case TK_MINUS:      return sugar_binop(astp, "sub");
    case TK_MULTIPLY:   return sugar_binop(astp, "mul");
    case TK_DIVIDE:     return sugar_binop(astp, "div");
    case TK_MOD:        return sugar_binop(astp, "mod");
    case TK_LSHIFT:     return sugar_binop(astp, "shl");
    case TK_RSHIFT:     return sugar_binop(astp, "shr");
    case TK_AND:        return sugar_binop(astp, "op_and");
    case TK_OR:         return sugar_binop(astp, "op_or");
    case TK_XOR:        return sugar_binop(astp, "op_xor");
    case TK_EQ:         return sugar_binop(astp, "eq");
    case TK_NE:         return sugar_binop(astp, "ne");
    case TK_LT:         return sugar_binop(astp, "lt");
    case TK_LE:         return sugar_binop(astp, "le");
    case TK_GE:         return sugar_binop(astp, "ge");
    case TK_GT:         return sugar_binop(astp, "gt");
    case TK_UNARY_MINUS:return sugar_unop(astp, "neg");
    case TK_NOT:        return sugar_unop(astp, "op_not");
    case TK_FFIDECL:
    case TK_FFICALL:    return sugar_ffi(ast);
    case TK_SEMI:       return sugar_semi(astp);
    case TK_LET:        return sugar_let(t, ast);
    default:            return AST_OK;
  }
}
