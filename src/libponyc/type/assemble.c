#include "assemble.h"
#include "assemble.h"
#include "subtype.h"
#include "lookup.h"
#include "../ast/token.h"
#include "../pass/names.h"
#include <assert.h>

static void append_one_to_union(ast_t* ast, ast_t* append)
{
  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    ast_t* next = ast_sibling(child);

    if(is_subtype(append, child))
    {
      // If the incoming type is a subtype of a type that is already in the
      // union, then do not bother to append it.
      return;
    } else if(is_subtype(child, append)) {
      // If a type in the union is a subtype of the incoming type, then remove
      // it from the union.
      ast_remove(child);
    }

    child = next;
  }

  ast_append(ast, append);
}

static void append_one_to_isect(ast_t* ast, ast_t* append)
{
  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    ast_t* next = ast_sibling(child);

    if(is_subtype(child, append))
    {
      // If the incoming type is a supertype of a type that is already in the
      // intersection, then do not bother to append it.
      return;
    } else if(is_subtype(append, child)) {
      // If a type in the intersection is a supertype of the incoming type,
      // then remove it from the intersection.
      ast_remove(child);
    }

    child = next;
  }

  ast_append(ast, append);
}

static void append_one_to_typeexpr(ast_t* ast, ast_t* append, bool is_union)
{
  if(is_union)
    append_one_to_union(ast, append);
  else
    append_one_to_isect(ast, append);
}

static void append_to_typeexpr(ast_t* ast, ast_t* append, bool is_union)
{
  if(ast_id(ast) == ast_id(append))
  {
    // Add each element of the typeexpr to the new typeexpr.
    ast_t* child = ast_child(append);

    while(child != NULL)
    {
      append_one_to_typeexpr(ast, child, is_union);
      child = ast_sibling(child);
    }
  } else {
    append_one_to_typeexpr(ast, append, is_union);
  }
}

static ast_t* type_typeexpr(token_id t, ast_t* l_type, ast_t* r_type)
{
  bool is_union = t == TK_UNIONTYPE;

  if(l_type == NULL)
    return r_type;

  if(r_type == NULL)
    return l_type;

  if(is_subtype(l_type, r_type))
  {
    if(is_union)
      return r_type;
    else
      return l_type;
  }

  if(is_subtype(r_type, l_type))
  {
    if(is_union)
      return l_type;
    else
      return r_type;
  }

  ast_t* type = ast_from(l_type, t);
  append_to_typeexpr(type, l_type, is_union);
  append_to_typeexpr(type, r_type, is_union);

  // If there's only one element, remove the type expression node.
  ast_t* child = ast_child(type);

  if(ast_sibling(child) == NULL)
  {
    child = ast_dup(child);
    ast_free_unattached(type);
    type = child;
  }

  return type;
}

static ast_t* type_base(ast_t* from, const char* package, const char* name)
{
  ast_t* ast = ast_from(from, TK_NOMINAL);
  ast_add(ast, ast_from(from, TK_NONE)); // ephemerality
  ast_add(ast, ast_from(from, TK_NONE)); // capability
  ast_add(ast, ast_from(from, TK_NONE)); // typeargs
  ast_add(ast, ast_from_string(from, name)); // name
  ast_add(ast, ast_from_string(from, package));

  return ast;
}

ast_t* type_builtin(ast_t* from, const char* name)
{
  ast_t* ast = type_base(from, NULL, name);

  if(!names_nominal(NULL, from, &ast))
  {
    ast_error(from, "unable to validate '%s'", name);
    ast_free(ast);
    return NULL;
  }

  return ast;
}

ast_t* type_pointer_to(ast_t* to)
{
  BUILD(pointer, to,
    NODE(TK_NOMINAL,
      NONE // Package
      ID("Pointer")
      NODE(TK_TYPEARGS,
        TREE(to)
        )
      NONE // Capability
      NONE // Ephemeral
      ));

  if(!names_nominal(NULL, to, &pointer))
  {
    ast_error(to, "unable to create Pointer[%s]", ast_print_type(to));
    ast_free(pointer);
    return NULL;
  }

  return pointer;
}

ast_t* type_sugar(ast_t* from, const char* package, const char* name)
{
  return type_base(from, package, name);
}

ast_t* type_union(ast_t* l_type, ast_t* r_type)
{
  return type_typeexpr(TK_UNIONTYPE, l_type, r_type);
}

ast_t* type_isect(ast_t* l_type, ast_t* r_type)
{
  return type_typeexpr(TK_ISECTTYPE, l_type, r_type);
}

ast_t* type_for_this(typecheck_t* t, ast_t* ast, token_id cap,
  token_id ephemeral)
{
  ast_t* id = ast_child(t->frame->type);
  ast_t* typeparams = ast_sibling(id);
  const char* name = ast_name(id);

  ast_t* nominal = ast_from(ast, TK_NOMINAL);
  ast_add(nominal, ast_from(ast, ephemeral));

  if(cap == TK_BOX)
    ast_add(nominal, ast_from(ast, TK_REF));
  else
    ast_add(nominal, ast_from(ast, cap));

  if(ast_id(typeparams) == TK_TYPEPARAMS)
  {
    ast_t* typeparam = ast_child(typeparams);
    ast_t* typeargs = ast_from(ast, TK_TYPEARGS);
    ast_add(nominal, typeargs);

    while(typeparam != NULL)
    {
      ast_t* typeparam_id = ast_child(typeparam);
      ast_t* typearg = type_sugar(ast, NULL, ast_name(typeparam_id));
      ast_append(typeargs, typearg);

      typeparam = ast_sibling(typeparam);
    }
  } else {
    ast_add(nominal, ast_from(ast, TK_NONE)); // empty typeargs
  }

  ast_add(nominal, ast_from_string(ast, name));
  ast_add(nominal, ast_from(ast, TK_NONE));

  if(cap != TK_BOX)
    return nominal;

  ast_t* arrow = ast_from(ast, TK_ARROW);
  ast_add(arrow, nominal);
  ast_add(arrow, ast_from(ast, TK_THISTYPE));
  return arrow;
}

ast_t* type_for_fun(ast_t* ast)
{
  ast_t* cap = ast_child(ast);
  ast_t* name = ast_sibling(cap);
  ast_t* typeparams = ast_sibling(name);
  ast_t* params = ast_sibling(typeparams);
  ast_t* result = ast_sibling(params);

  ast_t* fun = ast_from(ast, TK_FUNTYPE);
  ast_add(fun, result);
  ast_add(fun, params);
  ast_add(fun, typeparams);
  ast_add(fun, cap);

  return fun;
}

bool type_for_idseq(ast_t* idseq, ast_t* type)
{
  assert(ast_id(idseq) == TK_IDSEQ);
  ast_t* id = ast_child(idseq);

  if(ast_sibling(id) == NULL)
  {
    ast_settype(id, type);
    return true;
  }

  if(ast_id(type) != TK_TUPLETYPE)
  {
    ast_error(type, "must specify a tuple type for multiple identifiers");
    return false;
  }

  int index = 0;

  while(id != NULL)
  {
    ast_t* t = ast_childidx(type, index);

    if(t == NULL)
    {
      ast_error(type, "not enough types specified");
      return false;
    }

    ast_settype(id, t);
    id = ast_sibling(id);
    index++;
  }

  if(ast_childidx(type, index) != NULL)
  {
    ast_error(type, "too many types specified");
    return false;
  }

  return true;
}

bool flatten_union(ast_t** astp)
{
  ast_t* ast = *astp;
  ast_t* child = ast_child(ast);
  ast_t* type = NULL;

  while(child != NULL)
  {
    type = type_union(type, child);
    child = ast_sibling(child);
  }

  ast_replace(astp, type);
  return true;
}

bool flatten_isect(ast_t** astp)
{
  ast_t* ast = *astp;
  ast_t* child = ast_child(ast);
  ast_t* type = NULL;

  while(child != NULL)
  {
    type = type_isect(type, child);
    child = ast_sibling(child);
  }

  ast_replace(astp, type);
  return true;
}

ast_t* set_cap_and_ephemeral(ast_t* type, token_id cap, token_id ephemeral)
{
  switch(ast_id(type))
  {
    case TK_ISECTTYPE:
      // If the source is an intersection type, just set it for the first type
      // in the list. We're using the for subtyping, and we will be a subtype
      // of every type in the list.
      return set_cap_and_ephemeral(ast_child(type), cap, ephemeral);

    case TK_NOMINAL:
    {
      type = ast_dup(type);
      AST_GET_CHILDREN(type, package, id, typeargs, tcap, eph);

      ast_setid(tcap, cap);
      ast_setid(eph, ephemeral);
      return type;
    }

    case TK_TYPEPARAMREF:
    {
      type = ast_dup(type);
      AST_GET_CHILDREN(type, id, tcap, eph);

      ast_setid(tcap, cap);
      ast_setid(eph, ephemeral);
      return type;
    }

    case TK_ARROW:
      // Just use the lhs of the viewpoint type.
      return set_cap_and_ephemeral(ast_childidx(type, 1), cap, ephemeral);

    default: {}
  }

  assert(0);
  return NULL;
}
