#include "genheader.h"
#include "genname.h"
#include "gentype.h"
#include "../type/assemble.h"
#include "../type/lookup.h"
#include "../type/subtype.h"
#include "../type/reify.h"
#include <assert.h>

static const char* c_type_name(compile_t* c, gentype_t* g)
{
  if(g->type_name == c->str_Bool)
    return "bool";
  else if(g->type_name == c->str_I8)
    return "int8_t";
  else if(g->type_name == c->str_I16)
    return "int16_t";
  else if(g->type_name == c->str_I32)
    return "int32_t";
  else if(g->type_name == c->str_I64)
    return "int64_t";
  else if(g->type_name == c->str_I128)
    return "int128_t";
  else if(g->type_name == c->str_U8)
    return "uint8_t";
  else if(g->type_name == c->str_U16)
    return "uint16_t";
  else if(g->type_name == c->str_U32)
    return "uint32_t";
  else if(g->type_name == c->str_U64)
    return "uint64_t";
  else if(g->type_name == c->str_U128)
    return "uint128_t";
  else if(g->type_name == c->str_F32)
    return "float";
  else if(g->type_name == c->str_F64)
    return "double";

  return NULL;
}

static void print_type_name(compile_t* c, gentype_t* g)
{
  const char* c_name = c_type_name(c, g);

  if(c_name != NULL)
    fprintf(c->header, "%s", c_name);
  else
    fprintf(c->header, "%s*", g->type_name);
}

static void print_params(compile_t* c, ast_t* params)
{
  ast_t* param = ast_child(params);

  while(param != NULL)
  {
    AST_GET_CHILDREN(param, id, ptype);

    // Get a type for each parameter.
    gentype_t ptype_g;
    gentype(c, ptype, &ptype_g);

    // Print the parameter.
    fprintf(c->header, ", ");
    print_type_name(c, &ptype_g);
    fprintf(c->header, " %s", ast_name(id)); // TODO: trailing primes

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
  AST_GET_CHILDREN(fun, cap, id, typeparams, params, rtype, can_error, body);

  // Print the docstring if we have one.
  ast_t* first = ast_child(body);

  if((ast_id(first) == TK_STRING) && (ast_sibling(first) != NULL))
  {
    fprintf(c->header,
      "/*\n"
      "%s"
      "*/\n",
      ast_name(first)
      );
  }

  // Get a type for the result.
  gentype_t rtype_g;
  gentype(c, rtype, &rtype_g);

  // Print the function signature.
  print_type_name(c, &rtype_g);
  fprintf(c->header, " %s(", funname);

  print_type_name(c, g);
  fprintf(c->header, " self");

  print_params(c, params);

  fprintf(c->header, ");\n\n");
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
  // TODO: tuples, 128 bit int types, Pointer types
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
    fprintf(c->header,
      "/* Allocate a %s without initialising it. */\n%s* %s_Alloc();\n\n",
      g->type_name,
      g->type_name,
      g->type_name
      );
  }

  print_methods(c, g);
}
