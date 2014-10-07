#include "literal.h"
#include "../ast/token.h"
#include "../ds/stringtab.h"
#include "../pass/names.h"
#include "../type/subtype.h"
#include "../type/assemble.h"
#include "../type/assemble.h"
#include "../type/reify.h"
#include "../type/cap.h"
#include <string.h>
#include <assert.h>

bool expr_literal(ast_t* ast, const char* name)
{
  ast_t* type = type_builtin(ast, name);

  if(type == NULL)
    return false;

  ast_settype(ast, type);
  return true;
}

bool expr_this(ast_t* ast)
{
  // TODO: If in a recover expression, may not have access to "this".
  // Or we could lower it to tag, since it can't be assigned to. If in a
  // constructor, lower it to tag if not all fields are defined.
  ast_t* type = type_for_this(ast, cap_for_receiver(ast), false);
  ast_settype(ast, type);

  ast_t* nominal;

  if(ast_id(type) == TK_NOMINAL)
    nominal = type;
  else
    nominal = ast_childidx(type, 1);

  ast_t* typeargs = ast_childidx(nominal, 2);
  ast_t* typearg = ast_child(typeargs);

  while(typearg != NULL)
  {
    if(!expr_nominal(&typearg))
    {
      ast_error(ast, "couldn't create a type for 'this'");
      ast_free(type);
      return false;
    }

    typearg = ast_sibling(typearg);
  }

  if(!expr_nominal(&nominal))
  {
    ast_error(ast, "couldn't create a type for 'this'");
    ast_free(type);
    return false;
  }

  return true;
}

bool expr_tuple(ast_t* ast)
{
  ast_t* child = ast_child(ast);
  ast_t* type;

  if(ast_sibling(child) == NULL)
  {
    type = ast_type(child);
  } else {
    type = ast_from(ast, TK_TUPLETYPE);

    while(child != NULL)
    {
      ast_t* c_type = ast_type(child);

      if(c_type == NULL)
      {
        ast_error(child,
          "a tuple can't contain a control flow statement that never results "
          "in a value");
        return false;
      }

      ast_append(type, c_type);
      child = ast_sibling(child);
    }
  }

  ast_settype(ast, type);
  ast_inheriterror(ast);
  return true;
}

bool expr_error(ast_t* ast)
{
  if(ast_sibling(ast) != NULL)
  {
    ast_error(ast, "error must be the last expression in a sequence");
    ast_error(ast_sibling(ast), "error is followed with this expression");
    return false;
  }

  ast_seterror(ast);
  return true;
}

bool expr_compiler_intrinsic(ast_t* ast)
{
  ast_t* fun = ast_enclosing_method_body(ast);
  ast_t* body = ast_childidx(fun, 6);
  ast_t* child = ast_child(body);

  if((child != ast) || (ast_sibling(child) != NULL))
  {
    ast_error(ast, "a compiler intrinsic must be the entire body");
    return false;
  }

  ast_settype(ast, ast_from(ast, TK_COMPILER_INTRINSIC));
  return true;
}

bool expr_nominal(ast_t** astp)
{
  // Resolve typealiases and typeparam references.
  if(!names_nominal(*astp, astp))
    return false;

  ast_t* ast = *astp;

  if(ast_id(ast) != TK_NOMINAL)
    return true;

  // If still nominal, check constraints.
  ast_t* def = (ast_t*)ast_data(ast);

  // Special case: don't check the constraint of a Pointer. This allows a
  // Pointer[Pointer[A]], which is normally not allowed, as a Pointer[A] is
  // not a subtype of Any.
  ast_t* id = ast_child(def);
  const char* name = ast_name(id);

  if(!strcmp(name, "Pointer"))
    return true;

  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* typeargs = ast_childidx(ast, 2);

  return check_constraints(typeparams, typeargs);
}

bool expr_fun(ast_t* ast)
{
  ast_t* type = ast_childidx(ast, 4);
  ast_t* can_error = ast_sibling(type);
  ast_t* body = ast_sibling(can_error);

  if(ast_id(body) == TK_NONE)
    return true;

  ast_t* def = ast_enclosing_type(ast);
  bool is_trait = ast_id(def) == TK_TRAIT;

  // if specified, body type must match return type
  ast_t* body_type = ast_type(body);

  if(body_type == NULL)
  {
    ast_t* last = ast_childlast(body);
    ast_error(type, "function body always results in an error");
    ast_error(last, "function body expression is here");
    return false;
  }

  if(ast_id(body_type) == TK_COMPILER_INTRINSIC)
    return true;

  // check partial functions
  if(ast_id(can_error) == TK_QUESTION)
  {
    // if a partial function, check that we might actually error
    if(!is_trait && !ast_canerror(body))
    {
      ast_error(can_error, "function body is not partial but the function is");
      return false;
    }
  } else {
    // if not a partial function, check that we can't error
    if(ast_canerror(body))
    {
      ast_error(can_error, "function body is partial but the function is not");
      return false;
    }
  }

  if((ast_id(ast) == TK_FUN) && !is_subtype(body_type, type))
  {
    ast_t* last = ast_childlast(body);
    ast_error(type, "function body isn't a subtype of the result type");
    ast_error(last, "function body expression is here");
    return false;
  }

  return true;
}


static void propogate_coercion(ast_t* ast, ast_t* type)
{
  assert(ast != NULL);
  assert(type != NULL);

  if(!is_type_arith_literal(ast_type(ast)))
    return;

  ast_settype(ast, type);

  for(ast_t* p = ast_child(ast); p != NULL; p = ast_sibling(p))
    propogate_coercion(p, type);
}


bool is_type_arith_literal(ast_t* ast)
{
  assert(ast != NULL);
  token_id id = ast_id(ast);
  return (id == TK_INTLITERAL) || (id == TK_FLOATLITERAL);
}


static bool can_promote_literal(ast_t* ast, ast_t* target)
{
  assert(ast != NULL);

  if(target == NULL)
    return false;

  ast_print(target);

  if(ast_id(target) != TK_NOMINAL)
    return false;

  const char* type_name = ast_name(ast_childidx(target, 1));

  if(type_name == stringtab("I8") ||
    type_name == stringtab("I16") ||
    type_name == stringtab("I32") ||
    type_name == stringtab("I64") ||
    type_name == stringtab("I128") ||
    type_name == stringtab("U8") ||
    type_name == stringtab("U16") ||
    type_name == stringtab("U32") ||
    type_name == stringtab("U64") ||
    type_name == stringtab("U128"))
  {
    return (ast_id(ast_type(ast)) == TK_INTLITERAL);
  }

  if(type_name == stringtab("F32") ||
    type_name == stringtab("F64"))
    return true;

  return false;
}


bool promote_literal(ast_t* ast, ast_t* target_type)
{
  assert(ast != NULL);

  if(!can_promote_literal(ast, target_type))
  {
    ast_error(ast, "cannot determine type of literal");
    return false;
  }

  ast_t* prom_type = ast_dup(target_type);
  ast_t* cap = ast_childidx(prom_type, 3);
  assert(cap != NULL);
  ast_setid(cap, TK_TAG);

  propogate_coercion(ast, prom_type);
  return true;
}
