#include "sugar.h"
#include "../ds/stringtab.h"
#include <assert.h>

static ast_t* make_idseq(ast_t* ast, ast_t* id)
{
  ast_t* idseq = ast_from(ast, TK_IDSEQ);
  ast_add(idseq, id);
  return idseq;
}

static ast_t* make_var(ast_t* ast, ast_t* idseq, ast_t* type)
{
  ast_t* var = ast_from(ast, TK_VAR);
  ast_add(var, type);
  ast_add(var, idseq);
  return var;
}

static ast_t* make_ref(ast_t* ast, ast_t* id)
{
  ast_t* ref = ast_from(ast, TK_REFERENCE);
  ast_add(ref, id);
  return ref;
}

static ast_t* make_dot(ast_t* ast, ast_t* left, const char* right)
{
  ast_t* dot = ast_from(ast, TK_DOT);
  ast_t* r_right = ast_from_string(ast, stringtab(right));
  ast_add(dot, r_right);
  ast_add(dot, left);
  return dot;
}

static ast_t* make_call(ast_t* ast, ast_t* left)
{
  ast_t* tuple = ast_from(ast, TK_TUPLE);
  ast_add(tuple, ast_from(ast, TK_NONE));
  ast_add(tuple, ast_from(ast, TK_NONE));

  ast_t* call = ast_from(ast, TK_CALL);
  ast_add(call, tuple);
  ast_add(call, left);

  return call;
}

static ast_t* make_assign(ast_t* ast, ast_t* left, ast_t* right)
{
  ast_t* assign = ast_from(ast, TK_ASSIGN);
  ast_add(assign, right);
  ast_add(assign, left);
  return assign;
}

static bool sugar_for(ast_t* ast)
{
  assert(ast_id(ast) == TK_FOR);

  ast_t* for_idseq = ast_child(ast);
  ast_t* for_type = ast_sibling(for_idseq);
  ast_t* for_iter = ast_sibling(for_type);
  ast_t* for_body = ast_sibling(for_iter);
  ast_t* for_else = ast_sibling(for_body);

  ast_t* id = ast_hygienic_id(ast);
  const char* name = ast_name(id);

  ast_t* body = ast_from(ast, TK_SEQ);
  ast_scope(body);

  ast_add(body, for_body);
  ast_add(body,
    make_assign(ast,
      make_var(ast, for_idseq, for_type),
      make_call(ast, make_dot(ast, make_ref(ast, id), "next"))
      )
    );

  ast_t* whileloop = ast_from(ast, TK_WHILE);
  ast_scope(whileloop);

  ast_add(whileloop, for_else);
  ast_add(whileloop, body);
  ast_add(whileloop,
    make_call(ast, make_dot(ast, make_ref(ast, id), "has_next"))
    );

  ast_t* iter = make_assign(ast,
    make_var(ast, make_idseq(ast, id), ast_from(ast, TK_NONE)),
    for_iter
    );

  ast_t* seq = ast_from(ast, TK_SEQ);
  ast_scope(seq);
  ast_add(seq, whileloop);
  ast_add(seq, iter);

  ast_replace(ast, seq);

  if(!ast_set(seq, name, id))
  {
    assert(0);
    return false;
  }

  return true;
}

static bool sugar_case(ast_t* ast)
{
  assert(ast_id(ast) == TK_CASE);
  ast_t* body = ast_childidx(ast, 3);

  if(ast_id(body) != TK_NONE)
    return true;

  ast_t* next = ast;
  ast_t* next_body;

  do
  {
    next = ast_sibling(next);

    if(next == NULL)
    {
      ast_error(body, "case with no body has no following case body");
      return false;
    }

    assert(ast_id(next) == TK_CASE);
    next_body = ast_childidx(next, 3);
  } while(ast_id(next_body) == TK_NONE);

  ast_replace(body, next_body);
  return true;
}

static bool sugar_update(ast_t* ast)
{
  assert(ast_id(ast) == TK_ASSIGN);
  ast_t* call = ast_child(ast);
  ast_t* value = ast_sibling(call);

  if(ast_id(call) != TK_CALL)
    return true;

  ast_t* expr = ast_child(call);

  ast_t* positional = ast_sibling(expr);
  ast_t* named = ast_sibling(positional);
  positional = ast_dup(positional);

  if(ast_id(positional) == TK_NONE)
    ast_setid(positional, TK_POSITIONALARGS);

  ast_t* seq = ast_from(positional, TK_SEQ);
  ast_add(seq, value);
  ast_append(positional, seq);

  ast_t* dot = make_dot(ast, expr, "update");

  ast_t* update = ast_from(ast, TK_CALL);
  ast_add(update, named);
  ast_add(update, positional);
  ast_add(update, dot);

  ast_replace(ast, update);
  return true;
}

ast_result_t pass_sugar(ast_t* ast, int verbose)
{
  switch(ast_id(ast))
  {
    case TK_FOR:
      if(!sugar_for(ast))
        return AST_FATAL;
      break;

    case TK_BANG:
      // TODO: syntactic sugar for partial application
      break;

    case TK_CASE:
      if(!sugar_case(ast))
        return AST_FATAL;
      break;

    case TK_ASSIGN:
      if(!sugar_update(ast))
        return AST_FATAL;
      break;

    default: {}
  }

  return AST_OK;
}
