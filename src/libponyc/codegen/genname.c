#include "genname.h"
#include "../pkg/package.h"
#include "../ast/stringtab.h"
#include "../../libponyrt/mem/pool.h"
#include <string.h>
#include <assert.h>

static const char* build_name(const char* a, const char* b, ast_t* typeargs,
  bool function);

static void name_append(char* name, const char* append)
{
  strcat(name, "_");
  strcat(name, append);
}

static const char* typearg_name(ast_t* typearg)
{
  switch(ast_id(typearg))
  {
    case TK_UNIONTYPE:
      return build_name(NULL, "$union", typearg, false);

    case TK_ISECTTYPE:
      return build_name(NULL, "$isect", typearg, false);

    default: {}
  }

  return genname_type(typearg);
}

static size_t typeargs_len(ast_t* typeargs)
{
  if(typeargs == NULL)
    return 0;

  ast_t* typearg = ast_child(typeargs);
  size_t len = 0;

  while(typearg != NULL)
  {
    const char* argname = typearg_name(typearg);
    len += strlen(argname) + 1;
    typearg = ast_sibling(typearg);
  }

  return len;
}

static void typeargs_append(char* name, ast_t* typeargs, bool function)
{
  if(typeargs == NULL)
    return;

  if(function)
    strcat(name, "_");

  ast_t* typearg = ast_child(typeargs);

  while(typearg != NULL)
  {
    name_append(name, typearg_name(typearg));
    typearg = ast_sibling(typearg);
  }
}

static const char* build_name(const char* a, const char* b, ast_t* typeargs,
  bool function)
{
  size_t len = typeargs_len(typeargs);

  if(a != NULL)
    len += strlen(a) + 1;

  if(b != NULL)
    len += strlen(b) + 1;

  char* name = (char*)pool_alloc_size(len);

  if(a != NULL)
    strcpy(name, a);

  if(b != NULL)
  {
    if(a != NULL)
      name_append(name, b);
    else
      strcpy(name, b);
  }

  typeargs_append(name, typeargs, function);
  return stringtab_consume(name, len);
}

static const char* nominal_name(ast_t* ast)
{
  AST_GET_CHILDREN(ast, package, name, typeargs);

  ast_t* def = (ast_t*)ast_data(ast);
  ast_t* pkg = ast_nearest(def, TK_PACKAGE);
  const char* s = package_symbol(pkg);

  return build_name(s, ast_name(name), typeargs, false);
}

const char* genname_type(ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
      return stringtab("$object");

    case TK_TUPLETYPE:
      return build_name(NULL, "$tuple", ast, false);

    case TK_NOMINAL:
      return nominal_name(ast);

    default: {}
  }

  assert(0);
  return NULL;
}

const char* genname_typeid(const char* type)
{
  return build_name(type, "$id", NULL, false);
}

const char* genname_traitlist(const char* type)
{
  return build_name(type, "$traits", NULL, false);
}

const char* genname_fieldlist(const char* type)
{
  return build_name(type, "$fields", NULL, false);
}

const char* genname_trace(const char* type)
{
  return build_name(type, "$trace", NULL, false);
}

const char* genname_tracetuple(const char* type)
{
  return build_name(type, "$tracetuple", NULL, false);
}

const char* genname_serialise(const char* type)
{
  return build_name(type, "$serialise", NULL, false);
}

const char* genname_deserialise(const char* type)
{
  return build_name(type, "$deserialise", NULL, false);
}

const char* genname_dispatch(const char* type)
{
  return build_name(type, "$dispatch", NULL, false);
}

const char* genname_finalise(const char* type)
{
  return build_name(type, "_final", NULL, false);
}

const char* genname_descriptor(const char* type)
{
  return build_name(type, "$desc", NULL, false);
}

const char* genname_instance(const char* type)
{
  return build_name(type, "$inst", NULL, false);
}

const char* genname_fun(const char* type, const char* name, ast_t* typeargs)
{
  return build_name(type, name, typeargs, true);
}

const char* genname_be(const char* name)
{
  return build_name(name, "_send", NULL, false);
}

const char* genname_box(const char* name)
{
  return build_name(name, "$box", NULL, false);
}

const char* genname_unbox(const char* name)
{
  return build_name(name, "$unbox", NULL, false);
}
