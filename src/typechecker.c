#include "typechecker.h"
#include "type_scope.h"
#include "type_valid.h"
#include "type_expr.h"
#include <assert.h>

#if 0

static type_t* none;
static type_t* boolean;
static type_t* i8;
static type_t* i16;
static type_t* i32;
static type_t* i64;
static type_t* i128;
static type_t* u8;
static type_t* u16;
static type_t* u32;
static type_t* u64;
static type_t* u128;
static type_t* f32;
static type_t* f64;
static type_t* intlit;
static type_t* floatlit;
static type_t* string;
static type_t* comparable;
static type_t* ordered;






static bool infer_type(ast_t* ast, size_t idx, bool require)
{
  ast_t* type_ast = ast_childidx(ast, idx);
  type_t* type = ast_data(type_ast);
  ast_attach(ast, type);

  ast_t* expr_ast = ast_sibling(type_ast);
  type_t* expr = ast_data(expr_ast);

  if(require && (expr == infer))
  {
    ast_error(ast, "right-hand side has no type to assign");
    return false;
  }

  if(!type_sub(expr, type))
  {
    ast_error(ast, "the expression is not a subtype of the type");
    return false;
  }

  if(ast_id(type_ast) == TK_NONE) { ast_attach(ast, expr); }

  return true;
}

static bool unsigned_number(type_t* type)
{
  return (type == intlit) || (type == u8) || (type == u16) || (type == u32)
    || (type == u64) || (type == u128);
}

static bool signed_number(type_t* type)
{
  return (type == intlit) || (type == i8) || (type == i16) || (type == i32)
    || (type == i64) || (type == i128);
}

static bool int_number(type_t* type)
{
  return signed_number(type) || unsigned_number(type);
}

static bool real_number(type_t* type)
{
  return (type == floatlit) || (type == f32) || (type == f64);
}

static type_t* coerce_int(type_t* a, type_t* b)
{
  if(a == b)
  {
    return a;
  } else if((a == intlit) && int_number(b)) {
    return b;
  } else if((b == intlit) && int_number(a)) {
    return a;
  }

  return NULL;
}

static type_t* coerce_real(type_t* a, type_t* b)
{
  if(((a == floatlit) || (a == intlit)) && real_number(b))
  {
    return b;
  } else if(((b == floatlit) || (b == intlit)) && real_number(a)) {
    return a;
  }

  return NULL;
}

static type_t* coerce_number(type_t* a, type_t* b)
{
  type_t* r = coerce_int(a, b);
  if(r != NULL) { return r; }

  return coerce_real(a, b);
}

static bool same_number(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  type_t* ltype = ast_data(left);
  type_t* rtype = ast_data(right);

  type_t* r = coerce_number(ltype, rtype);

  if(r == NULL)
  {
    ast_error(ast, "both sides must be the same Number type");
    return false;
  }

  ast_attach(ast, r);
  return true;
}

static bool conditional(ast_t* ast, int idx)
{
  ast_t* cond = ast_childidx(ast, idx);

  if(!type_sub(ast_data(cond), boolean))
  {
    ast_error(ast, "conditional must be a Bool");
    return false;
  }

  return true;
}

static bool upper_bounds(ast_t* ast, int idx1, int idx2)
{
  ast_t* branch1 = ast_childidx(ast, idx1);
  ast_t* branch2 = ast_childidx(ast, idx2);

  if(ast_id(branch2) == TK_NONE)
  {
    ast_attach(ast, none);
    return true;
  }

  type_t* type1 = ast_data(branch1);
  type_t* type2 = ast_data(branch2);

  if(type_sub(type1, type2))
  {
    ast_attach(ast, type2);
  } else if(type_sub(type2, type1)) {
    ast_attach(ast, type1);
  } else {
    ast_error(ast, "branches have unrelated types");
    return false;
  }

  return true;
}

static bool trait_and_subtype(ast_t* ast, type_t* trait, const char* name)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  type_t* ltype = ast_data(left);
  type_t* rtype = ast_data(right);

  if(!type_sub(ltype, trait))
  {
    ast_error(ast, "left-hand side must be %s", name);
    return false;
  }

  if(!type_sub(rtype, ltype))
  {
    ast_error(ast, "right-hand side must be a subtype of the left-hand side");
    return false;
  }

  ast_attach(ast, boolean);
  return true;
}

static bool def_before_use(ast_t* def, ast_t* use)
{
  if(ast_id(def) != TK_LOCAL) { return true; }

  size_t line0 = ast_line(def);
  size_t pos0 = ast_pos(def);

  size_t line1 = ast_line(use);
  size_t pos1 = ast_pos(use);

  if((line0 > line1)
    || ((line0 == line1) && (pos0 > pos1))
    )
  {
    ast_error(def, "identifier %s is used before it is defined",
      ast_name(use));
    return false;
  }

  return true;
}

static bool body_is_result(ast_t* ast, int idx, ast_t* body)
{
  ast_t* result = ast_childidx(ast, idx);
  type_t* rtype = ast_data(result);
  type_t* btype = ast_data(body);

  if(rtype == infer) { rtype = none; }

  if(ast_id(body) != TK_NONE)
  {
    if(coerce_number(btype, rtype)) { return true; }

    if(!type_sub(btype, rtype))
    {
      ast_error(body, "body is not the result type");
      return false;
    }
  }

  return true;
}

static bool type_first(ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_BE:
      ast_attach(ast, none);
      return true;

    case TK_FIELD:
      ast_attach(ast, ast_data(ast_childidx(ast, 2)));
      break;

    case TK_INFER:
    case TK_ADT:
    case TK_OBJTYPE:
    case TK_FUNTYPE:
    {
      type_t* type = type_ast(ast);
      if(type == NULL) { return false; }
      ast_attach(ast, type);
      return true;
    }

    case TK_THIS:
// FIX:      ast_attach(ast, type_name(ast, "This"));
      return true;

    case TK_STRING:
      ast_attach(ast, string);
      return true;

    case TK_INT:
      ast_attach(ast, intlit);
      return true;

    case TK_FLOAT:
      ast_attach(ast, floatlit);
      return true;

    case TK_BREAK:
    case TK_CONTINUE:
    {
      if(!ast_nearest(ast, TK_WHILE)
        && !ast_nearest(ast, TK_DO)
        && !ast_nearest(ast, TK_FOR)
        )
      {
        ast_error(ast, "%s can only appear in a loop", ast_name(ast));
        return false;
      }

      ast_attach(ast, infer);
      return true;
    }

    case TK_ERROR:
    {
      if(!ast_nearest(ast, TK_CANTHROW))
      {
        ast_error(ast,
          "undef must be in a try or in a partial function/lambda");
        return false;
      }

      ast_attach(ast, infer);
      return true;
    }

    default: {}
  }

  return true;
}

static bool type_second(ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_TYPEPARAM:
    case TK_PARAM:
      return infer_type(ast, 1, false);

    case TK_FUN:
      return body_is_result(ast, 5, ast_childidx(ast, 6));

    case TK_LOCAL:
      return infer_type(ast, 2, true);

    case TK_LAMBDA:
      ast_attach(ast, type_ast(ast));
      return body_is_result(ast, 3, ast_childidx(ast, 4));

    case TK_REF:
    {
      ast_t* id = ast_child(ast);
      const char* name = ast_name(id);
      ast_t* def = ast_get(ast, name);

      if((name[0] >= 'A') && (name[0] <= 'Z'))
      {
        if(def == NULL)
        {
          ast_error(id, "type %s is not in scope", name);
          return false;
        }

        /*
        FIX: it's a constructor, parent could be:
        typeargs:
          if parent isn't a call, it's a static apply
        call:
          static apply, if it has parameters
          apply, if static apply doesn't exist or has no parameters
            implies a static apply with no arguments comes first
        anything else:
          static apply, no arguments
        */
      } else {
        if(def == NULL)
        {
          ast_error(id, "identifier %s is not in scope", name);
          return false;
        }

        // FIX: local, param, field or function
        // could be a function in a trait
        switch(ast_id(def))
        {
          case TK_LOCAL:
            if(!def_before_use(def, id)) { return false; }
            ast_attach(ast, ast_data(def));
            break;

          case TK_PARAM:
          case TK_FIELD:
            assert(ast_data(def) != NULL);
            ast_attach(ast, ast_data(def));
            break;

          case TK_FUN:
          case TK_BE:
          case TK_NEW:
            /*
            FIX: parent could be:
            call: it's a function call or partial application
            typeargs: parent's parent must be call
            anything else: it's partial application
            */
            break;

          default: {}
        }
      }

      return true;
    }

    case TK_CANTHROW:
      ast_attach(ast, ast_data(ast_child(ast)));
      return true;

    case TK_SEQ:
    {
      // the type of a sequence is the type of the last expression
      size_t n = ast_childcount(ast);
      ast_t* last = ast_childidx(ast, n - 1);
      ast_attach(ast, ast_data(last));
      return true;
    }

    case TK_DOT:
      // FIX:
      return true;

    case TK_TYPEARGS:
      // FIX:
      return true;

    case TK_CALL:
      // FIX:
      return true;

    case TK_ASSIGN:
      // FIX:
      return true;

    case TK_NOT:
    {
      ast_t* child = ast_child(ast);
      type_t* type = ast_data(child);

      if(type_sub(type, boolean))
      {
        ast_attach(ast, boolean);
      } else if(int_number(type)) {
        ast_attach(ast, type);
      } else {
        ast_error(ast, "must be a Bool or an integer");
        return false;
      }

      return true;
    }

    case TK_MINUS:
    {
      ast_t* left = ast_child(ast);
      ast_t* right = ast_sibling(left);
      type_t* ltype = ast_data(left);

      if(right != NULL)
      {
        return same_number(ast);
      } else if(signed_number(ltype)) {
        ast_attach(ast, ltype);
      } else {
        ast_error(ast, "must be a signed integer");
        return false;
      }

      return true;
    }

    case TK_AND:
    case TK_OR:
    case TK_XOR:
    {
      ast_t* left = ast_child(ast);
      ast_t* right = ast_sibling(left);
      type_t* ltype = ast_data(left);
      type_t* rtype = ast_data(right);

      if(type_sub(ltype, boolean) && type_sub(rtype, boolean))
      {
        ast_attach(ast, boolean);
        return true;
      }

      type_t* r = coerce_number(ltype, rtype);

      if(r == NULL)
      {
        ast_error(ast, "both sides must either be Bool or the same integer");
        return false;
      }

      ast_attach(ast, r);
      return true;
    }

    case TK_PLUS:
    case TK_MULTIPLY:
    case TK_DIVIDE:
    case TK_MOD:
      return same_number(ast);

    case TK_LSHIFT:
    case TK_RSHIFT:
    {
      // left must be Integer, right Unsigned, result is the type of left
      ast_t* left = ast_child(ast);
      ast_t* right = ast_sibling(left);
      type_t* ltype = ast_data(left);
      type_t* rtype = ast_data(right);

      if(!int_number(ltype))
      {
        ast_error(ast, "left-hand side must be an integer");
        return false;
      }

      if(!unsigned_number(rtype))
      {
        ast_error(ast, "right-hand side must be an unsigned integer");
        return false;
      }

      ast_attach(ast, ltype);
      return true;
    }

    case TK_EQ:
    case TK_NE:
      return trait_and_subtype(ast, comparable, "Comparable");

    case TK_LT:
    case TK_LE:
    case TK_GE:
    case TK_GT:
      return trait_and_subtype(ast, ordered, "Ordered");

    case TK_IF:
      return conditional(ast, 0) && upper_bounds(ast, 1, 2);

    case TK_MATCH:
      // FIX:
      return true;

    case TK_CASE:
      // FIX:
      return true;

    case TK_AS:
      // FIX:
      return true;

    case TK_WHILE:
      ast_attach(ast, none);
      return conditional(ast, 0);

    case TK_DO:
      ast_attach(ast, none);
      return conditional(ast, 1);

    case TK_FOR:
      // FIX:
      ast_attach(ast, none);
      return true;

    case TK_TRY:
      return upper_bounds(ast, 0, 1);

    case TK_RETURN:
    {
      ast_attach(ast, ast_data(ast_child(ast)));
      ast_t* fun;

      if(((fun = ast_nearest(ast, TK_FUN)) != NULL)
        || ((fun = ast_nearest(ast, TK_BE)) != NULL)
        )
      {
        return body_is_result(fun, 6, ast);
      } else if((fun = ast_nearest(ast, TK_LAMBDA)) != NULL) {
        return body_is_result(fun, 3, ast);
      }

      return true;
    }

    default: {}
  }

  return true;
}

#endif

bool typecheck(ast_t* ast)
{
  if(!ast_visit(ast, type_scope, NULL))
    return false;

  if(!ast_visit(ast, type_valid, type_expr))
    return false;

  return true;
}

bool is_type_id(const char* s)
{
  int i = 0;

  if(s[i] == '_')
    i++;

  return (s[i] >= 'A') && (s[i] <= 'Z');
}
