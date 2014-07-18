#include "genname.h"
#include "../ds/stringtab.h"
#include <string.h>
#include <stdlib.h>

static void name_append(char* name, const char* append)
{
  strcat(name, "_");
  strcat(name, append);
}

static size_t typeargs_len(ast_t* typeargs)
{
  if(typeargs == NULL)
    return 0;

  ast_t* typearg = ast_child(typeargs);
  size_t len = 0;

  while(typearg != NULL)
  {
    const char* argname = codegen_typename(typearg);

    if(argname == NULL)
      return -1;

    len += strlen(argname) + 1;
    typearg = ast_sibling(typearg);
  }

  return len;
}

static void typeargs_append(char* name, ast_t* typeargs)
{
  if(typeargs == NULL)
    return;

  ast_t* typearg = ast_child(typeargs);

  while(typearg != NULL)
  {
    name_append(name, codegen_typename(typearg));
    typearg = ast_sibling(typearg);
  }
}

static const char* build_name(const char* a, const char* b, ast_t* typeargs)
{
  size_t len = strlen(a) + strlen(b) + 2;
  size_t tlen = typeargs_len(typeargs);

  if(tlen == -1)
    return NULL;

  len += tlen;

  char* name = malloc(len);
  strcpy(name, a);
  name_append(name, b);
  typeargs_append(name, typeargs);

  const char* result = stringtab(name);
  free(name);
  return result;
}

static const char* nominal_name(ast_t* ast)
{
  ast_t* package = ast_child(ast);
  ast_t* name = ast_sibling(package);
  ast_t* typeargs = ast_sibling(name);

  return build_name(ast_name(package), ast_name(name), typeargs);
}

const char* codegen_typename(ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_NOMINAL:
      return nominal_name(ast);

    default:
    {
      ast_error(ast, "not implemented (name for non-nominal type)");
      break;
    }
  }

  return NULL;
}

const char* codegen_funname(ast_t* type, const char* name, ast_t* typeargs)
{
  return build_name(codegen_typename(type), name, typeargs);
}
