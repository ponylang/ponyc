#include "genheader.h"
#include "genname.h"
#include "../type/assemble.h"
#include "../type/lookup.h"
#include "../type/subtype.h"
#include "../type/reify.h"
#include "../ast/printbuf.h"
#include "../../libponyrt/mem/pool.h"
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
  else if(name == c->str_ILong)
    return "long";
  else if(name == c->str_ISize)
    return "ssize_t";
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
  else if(name == c->str_ULong)
    return "unsigned long";
  else if(name == c->str_USize)
    return "size_t";
  else if(name == c->str_F32)
    return "float";
  else if(name == c->str_F64)
    return "double";

  return NULL;
}

static void print_base_type(compile_t* c, printbuf_t* buf, ast_t* type)
{
  if(ast_id(type) == TK_NOMINAL)
  {
    const char* name = genname_type(type);
    const char* c_name = c_type_name(c, name);

    if(c_name != NULL)
      printbuf(buf, c_name);
    else
      printbuf(buf, "%s*", name);
  } else {
    printbuf(buf, "void*");
  }
}

static int print_pointer_type(compile_t* c, printbuf_t* buf, ast_t* type)
{
  ast_t* typeargs = ast_childidx(type, 2);
  ast_t* elem = ast_child(typeargs);

  if(is_pointer(elem) || is_maybe(elem))
    return print_pointer_type(c, buf, elem) + 1;

  print_base_type(c, buf, elem);
  return 1;
}

static void print_type_name(compile_t* c, printbuf_t* buf, ast_t* type)
{
  if(is_pointer(type) || is_maybe(type))
  {
    int depth = print_pointer_type(c, buf, type);

    for(int i = 0; i < depth; i++)
      printbuf(buf, "*");
  } else {
    print_base_type(c, buf, type);
  }
}

static void print_params(compile_t* c, printbuf_t* buf, ast_t* params)
{
  ast_t* param = ast_child(params);

  while(param != NULL)
  {
    AST_GET_CHILDREN(param, id, ptype);

    // Print the parameter.
    printbuf(buf, ", ");
    print_type_name(c, buf, ptype);

    // Smash trailing primes to underscores.
    const char* name = ast_name(id);
    size_t len = strlen(name) + 1;
    size_t buf_size = len;
    char* buffer = (char*)pool_alloc_size(buf_size);
    memcpy(buffer, name, len);

    len--;

    while(buffer[--len] == '\'')
      buffer[len] = '_';

    printbuf(buf, " %s", buffer);

    param = ast_sibling(param);
    pool_free_size(buf_size, buffer);
  }
}

static ast_t* get_fun(ast_t* type, const char* name, ast_t* typeargs)
{
  ast_t* this_type = set_cap_and_ephemeral(type, TK_REF, TK_NONE);
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

static void print_method(compile_t* c, printbuf_t* buf, reachable_type_t* t,
  const char* name, ast_t* typeargs)
{
  const char* funname = genname_fun(t->name, name, typeargs);
  LLVMValueRef func = LLVMGetNamedFunction(c->module, funname);

  if(func == NULL)
    return;

  // Get a reified function.
  ast_t* fun = get_fun(t->type, name, typeargs);

  if(fun == NULL)
    return;

  AST_GET_CHILDREN(fun, cap, id, typeparams, params, rtype, can_error, body,
    docstring);

  // Print the docstring if we have one.
  if(ast_id(docstring) == TK_STRING)
  {
    printbuf(buf,
      "/*\n"
      "%s"
      "*/\n",
      ast_name(docstring)
      );
  }

  // Print the function signature.
  print_type_name(c, buf, rtype);
  printbuf(buf, " %s", funname);

  switch(ast_id(fun))
  {
    case TK_NEW:
    case TK_BE:
    {
      ast_t* def = (ast_t*)ast_data(t->type);

      if(ast_id(def) == TK_ACTOR)
        printbuf(buf, "__send");

      break;
    }

    default: {}
  }

  printbuf(buf, "(");
  print_type_name(c, buf, t->type);
  printbuf(buf, " self");

  print_params(c, buf, params);

  printbuf(buf, ");\n\n");
  ast_free_unattached(fun);
}

static void print_methods(compile_t* c, reachable_type_t* t, printbuf_t* buf)
{
  size_t i = HASHMAP_BEGIN;
  reachable_method_name_t* n;

  while((n = reachable_method_names_next(&t->methods, &i)) != NULL)
  {
    size_t j = HASHMAP_BEGIN;
    reachable_method_t* m;

    while((m = reachable_methods_next(&n->r_methods, &j)) != NULL)
      print_method(c, buf, t, n->name, m->typeargs);
  }
}

static void print_types(compile_t* c, FILE* fp, printbuf_t* buf)
{
  size_t i = HASHMAP_BEGIN;
  reachable_type_t* t;

  while((t = reachable_types_next(c->reachable, &i)) != NULL)
  {
    // Print the docstring if we have one.
    ast_t* def = (ast_t*)ast_data(t->type);
    ast_t* docstring = ast_childidx(def, 6);

    if(ast_id(docstring) == TK_STRING)
      fprintf(fp, "/*\n%s*/\n", ast_name(docstring));

    if(!is_pointer(t->type) && !is_maybe(t->type) && !is_machine_word(t->type))
    {
      // Forward declare an opaque type.
      fprintf(fp, "typedef struct %s %s;\n\n", t->name, t->name);

      // Function signature for the allocator.
      printbuf(buf,
        "/* Allocate a %s without initialising it. */\n%s* %s_Alloc();\n\n",
        t->name,
        t->name,
        t->name
        );
    }

    print_methods(c, t, buf);
  }
}

bool genheader(compile_t* c)
{
  // Open a header file.
  const char* file_h = suffix_filename(c->opt->output, "", c->filename, ".h");
  FILE* fp = fopen(file_h, "wt");

  if(fp == NULL)
  {
    errorf(NULL, "couldn't write to %s", file_h);
    return false;
  }

  fprintf(fp,
    "#ifndef pony_%s_h\n"
    "#define pony_%s_h\n"
    "\n"
    "/* This is an auto-generated header file. Do not edit. */\n"
    "\n"
    "#include <stdint.h>\n"
    "#include <stdbool.h>\n"
    "\n"
    "#ifdef __cplusplus\n"
    "extern \"C\" {\n"
    "#endif\n"
    "\n"
    "#ifdef _MSC_VER\n"
    "typedef struct __int128_t { uint64_t low; int64_t high; } __int128_t;\n"
    "typedef struct __uint128_t { uint64_t low; uint64_t high; } "
      "__uint128_t;\n"
    "#endif\n"
    "\n",
    c->filename,
    c->filename
    );

  printbuf_t* buf = printbuf_new();
  print_types(c, fp, buf);
  fwrite(buf->m, 1, buf->offset, fp);
  printbuf_free(buf);

  fprintf(fp,
    "\n"
    "#ifdef __cplusplus\n"
    "}\n"
    "#endif\n"
    "\n"
    "#endif\n"
    );

  fclose(fp);
  return true;
}
