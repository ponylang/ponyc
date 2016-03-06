#include "genname.h"
#include "../pkg/package.h"
#include "../ast/stringtab.h"
#include "../../libponyrt/mem/pool.h"
#include <string.h>
#include <assert.h>

static const char* build_name(const char* a, const char* b, ast_t* typeargs,
  ast_t* cap, bool use_cap, bool function);

static const char* nominal_name(ast_t* ast, bool use_cap)
{
  AST_GET_CHILDREN(ast, package, name, typeargs, cap, eph);

  ast_t* def = (ast_t*)ast_data(ast);
  ast_t* pkg = ast_nearest(def, TK_PACKAGE);

  if(!use_cap)
    cap = NULL;

  return build_name(package_symbol(pkg), ast_name(name), typeargs, cap,
    true, false);
}

static void name_append(char* name, const char* append)
{
  strcat(name, "_");
  strcat(name, append);
}

static const char* element_name(ast_t* type, bool use_cap)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
      return build_name(NULL, "$union", type, NULL, true, false);

    case TK_ISECTTYPE:
      return build_name(NULL, "$isect", type, NULL, true, false);

    case TK_TUPLETYPE:
      return build_name(NULL, "$tuple", type, NULL, true, false);

    case TK_NOMINAL:
      return nominal_name(type, use_cap);

    default: {}
  }

  assert(0);
  return NULL;
}

static size_t elements_len(ast_t* elements, bool use_cap, bool function)
{
  if(elements == NULL)
    return 0;

  ast_t* element = ast_child(elements);
  size_t len = 0;

  if(function)
    len++;

  while(element != NULL)
  {
    const char* name = element_name(element, use_cap);
    len += strlen(name) + 1;
    element = ast_sibling(element);
  }

  return len;
}

static void elements_append(char* name, ast_t* elements, bool use_cap,
  bool function)
{
  if(elements == NULL)
    return;

  if(function)
    strcat(name, "_");

  ast_t* element = ast_child(elements);

  while(element != NULL)
  {
    name_append(name, element_name(element, use_cap));
    element = ast_sibling(element);
  }
}

static const char* build_name(const char* a, const char* b, ast_t* elements,
  ast_t* cap, bool use_cap, bool function)
{
  size_t len = elements_len(elements, use_cap, function);
  const char* cap_str = NULL;

  if(a != NULL)
    len += strlen(a) + 1;

  if(b != NULL)
    len += strlen(b) + 1;

  if(cap != NULL)
  {
    cap_str = ast_get_print(cap);
    len += strlen(cap_str) + 1;
  }

  char* name = (char*)ponyint_pool_alloc_size(len);

  if(a != NULL)
    strcpy(name, a);

  if(b != NULL)
  {
    if(a != NULL)
      name_append(name, b);
    else
      strcpy(name, b);
  }

  elements_append(name, elements, use_cap, function);

  if(cap != NULL)
    name_append(name, cap_str);

  return stringtab_consume(name, len);
}

const char* genname_type(ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
      return stringtab("$object");

    case TK_TUPLETYPE:
      return build_name(NULL, "$tuple", ast, NULL, false, false);

    case TK_NOMINAL:
      return nominal_name(ast, false);

    default: {}
  }

  assert(0);
  return NULL;
}

const char* genname_typeid(const char* type)
{
  return build_name(type, "$id", NULL, NULL, false, false);
}

const char* genname_traitlist(const char* type)
{
  return build_name(type, "$traits", NULL, NULL, false, false);
}

const char* genname_fieldlist(const char* type)
{
  return build_name(type, "$fields", NULL, NULL, false, false);
}

const char* genname_trace(const char* type)
{
  return build_name(type, "$trace", NULL, NULL, false, false);
}

const char* genname_serialise(const char* type)
{
  return build_name(type, "$serialise", NULL, NULL, false, false);
}

const char* genname_deserialise(const char* type)
{
  return build_name(type, "$deserialise", NULL, NULL, false, false);
}

const char* genname_dispatch(const char* type)
{
  return build_name(type, "$dispatch", NULL, NULL, false, false);
}

const char* genname_finalise(const char* type)
{
  return build_name(type, "_final", NULL, NULL, false, false);
}

const char* genname_descriptor(const char* type)
{
  return build_name(type, "$desc", NULL, NULL, false, false);
}

const char* genname_instance(const char* type)
{
  return build_name(type, "$inst", NULL, NULL, false, false);
}

const char* genname_fun(const char* type, const char* name, ast_t* typeargs)
{
  return build_name(type, name, typeargs, NULL, false, true);
}

const char* genname_be(const char* name)
{
  return build_name(name, "_send", NULL, NULL, false, false);
}

const char* genname_box(const char* name)
{
  return build_name(name, "$box", NULL, NULL, false, false);
}

const char* genname_unbox(const char* name)
{
  return build_name(name, "$unbox", NULL, NULL, false, false);
}
