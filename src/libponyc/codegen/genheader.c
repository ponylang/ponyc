#include "genheader.h"
#include "genname.h"
#include "../type/assemble.h"
#include "../type/lookup.h"
#include "../type/subtype.h"
#include "../type/reify.h"
#include "../ast/printbuf.h"
#include <string.h>
#include <assert.h>

static const char* c_type_name(compile_t* c, const char* name)
{
  if(name == c->str_Bool)
    return "bool";
  else if(name == c->str_I8)
    return "int8_t";
  else if(name == c->str_I16)
    return "int16_t";
  else if(name == c->str_I32)
    return "int32_t";
  else if(name == c->str_I64)
    return "int64_t";
  else if(name == c->str_I128)
    return "__int128_t";
  else if(name == c->str_U8)
    return "char";
  else if(name == c->str_U16)
    return "uint16_t";
  else if(name == c->str_U32)
    return "uint32_t";
  else if(name == c->str_U64)
    return "uint64_t";
  else if(name == c->str_U128)
    return "__uint128_t";
  else if(name == c->str_F32)
    return "float";
  else if(name == c->str_F64)
    return "double";

  return NULL;
}

static void print_base_type(compile_t* c, ast_t* type)
{
  if(ast_id(type) == TK_NOMINAL)
  {
    const char* name = genname_type(type);
    const char* c_name = c_type_name(c, name);

    if(c_name != NULL)
      printbuf(c->header_buf, c_name);
    else
      printbuf(c->header_buf, "%s*", name);
  } else {
    printbuf(c->header_buf, "void*");
  }
}

static int print_pointer_type(compile_t* c, ast_t* type)
{
  ast_t* typeargs = ast_childidx(type, 2);
  ast_t* elem = ast_child(typeargs);

  if(is_pointer(elem))
    return print_pointer_type(c, elem) + 1;

  print_base_type(c, elem);
  return 1;
}

static void print_type_name(compile_t* c, ast_t* type)
{
  if(is_pointer(type))
  {
    printbuf(c->header_buf, "const ");
    int depth = print_pointer_type(c, type);

    for(int i = 0; i < depth; i++)
      printbuf(c->header_buf, "*");
  } else {
    print_base_type(c, type);
  }
}

static void print_params(compile_t* c, ast_t* params)
{
  ast_t* param = ast_child(params);

  while(param != NULL)
  {
    AST_GET_CHILDREN(param, id, ptype);

    // Print the parameter.
    printbuf(c->header_buf, ", ");
    print_type_name(c, ptype);

    // Smash trailing primes to underscores.
    const char* name = ast_name(id);
    size_t len = strlen(name) + 1;
    VLA(char, buf, len);
    memcpy(buf, name, len);

    len--;

    while(buf[--len] == '\'')
      buf[len] = '_';

    printbuf(c->header_buf, " %s", buf);

    param = ast_sibling(param);
  }
}

static ast_t* get_fun(gentype_t* g, const char* name, ast_t* typeargs)
{
  ast_t* this_type = set_cap_and_ephemeral(g->ast, TK_REF, TK_NONE);
  ast_t* fun = lookup(NULL, NULL, this_type, name);
  ast_free_unattached(this_type);
  assert(fun != NULL);

  if(typeargs != NULL)
  {
    ast_t* typeparams = ast_childidx(fun, 2);
    ast_t* r_fun = reify(fun, typeparams, typeargs);
    ast_free_unattached(fun);
    fun = r_fun;
    assert(fun != NULL);
  }

  // No signature for any function with a tuple argument or return value, or
  // any function that might raise an error.
  AST_GET_CHILDREN(fun, cap, id, typeparams, params, result, can_error);

  if(ast_id(can_error) == TK_QUESTION)
    return NULL;

  if(ast_id(result) == TK_TUPLETYPE)
    return NULL;

  ast_t* param = ast_child(params);

  while(param != NULL)
  {
    AST_GET_CHILDREN(param, p_id, p_type);

    if(ast_id(p_type) == TK_TUPLETYPE)
      return NULL;

    param = ast_sibling(param);
  }

  return fun;
}

static void print_method(compile_t* c, gentype_t* g, const char* name,
  ast_t* typeargs)
{
  const char* funname = genname_fun(g->type_name, name, typeargs);
  LLVMValueRef func = LLVMGetNamedFunction(c->module, funname);

  if(func == NULL)
    return;

  // Get a reified function.
  ast_t* fun = get_fun(g, name, typeargs);

  if(fun == NULL)
    return;

  AST_GET_CHILDREN(fun, cap, id, typeparams, params, rtype, can_error, body,
    docstring);

  // Print the docstring if we have one.
  if(ast_id(docstring) == TK_STRING)
  {
    printbuf(c->header_buf,
      "/*\n"
      "%s"
      "*/\n",
      ast_name(docstring)
      );
  }

  // Print the function signature.
  print_type_name(c, rtype);
  printbuf(c->header_buf, " %s(", funname);

  print_type_name(c, g->ast);
  printbuf(c->header_buf, " self");

  print_params(c, params);

  printbuf(c->header_buf, ");\n\n");
  ast_free_unattached(fun);
}

static void print_methods(compile_t* c, gentype_t* g)
{
  ast_t* def = (ast_t*)ast_data(g->ast);
  ast_t* members = ast_childidx(def, 4);
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_NEW:
      case TK_BE:
      case TK_FUN:
      {
        AST_GET_CHILDREN(member, cap, id);
        print_method(c, g, ast_name(id), NULL);
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }
}

void genheader(compile_t* c, gentype_t* g)
{
  if(g->primitive != NULL)
    return;

  // Print the docstring if we have one.
  ast_t* def = (ast_t*)ast_data(g->ast);
  ast_t* docstring = ast_childidx(def, 6);

  if(ast_id(docstring) == TK_STRING)
    fprintf(c->header, "/*\n%s*/\n", ast_name(docstring));

  // Forward declare an opaque type.
  fprintf(c->header, "typedef struct %s %s;\n\n", g->type_name, g->type_name);

  if(!is_pointer(g->ast))
  {
    // Function signature for the allocator.
    printbuf(c->header_buf,
      "/* Allocate a %s without initialising it. */\n%s* %s_Alloc();\n\n",
      g->type_name,
      g->type_name,
      g->type_name
      );
  }

  print_methods(c, g);
}
