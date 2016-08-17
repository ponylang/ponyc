#include "genname.h"
#include "../pkg/package.h"
#include "../ast/stringtab.h"
#include "../ast/lexer.h"
#include "../../libponyrt/mem/pool.h"
#include <string.h>
#include <assert.h>

static void types_append(printbuf_t* buf, ast_t* elements);

static void type_append(printbuf_t* buf, ast_t* type, bool first)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    {
      // 3u_Arg1_Arg2_Arg3
      printbuf(buf, "%du", ast_childcount(type));
      types_append(buf, type);
      return;
    }

    case TK_ISECTTYPE:
    {
      // 3i_Arg1_Arg2_Arg3
      printbuf(buf, "%di", ast_childcount(type));
      types_append(buf, type);
      return;
    }

    case TK_TUPLETYPE:
    {
      // 3t_Arg1_Arg2_Arg3
      printbuf(buf, "%dt", ast_childcount(type));
      types_append(buf, type);
      return;
    }

    case TK_NOMINAL:
    {
      // pkg_Type[_Arg1_Arg2]_cap
      AST_GET_CHILDREN(type, package, name, typeargs, cap, eph);

      ast_t* def = (ast_t*)ast_data(type);
      ast_t* pkg = ast_nearest(def, TK_PACKAGE);
      const char* pkg_name = package_symbol(pkg);

      if(pkg_name != NULL)
        printbuf(buf, "%s_", pkg_name);

      printbuf(buf, "%s", ast_name(name));
      types_append(buf, typeargs);

      if(!first)
        printbuf(buf, "_%s", ast_get_print(cap));

      return;
    }

    default: {}
  }

  assert(0);
}

static void types_append(printbuf_t* buf, ast_t* elements)
{
  // _Arg1_Arg2
  if(elements == NULL)
    return;

  ast_t* elem = ast_child(elements);

  while(elem != NULL)
  {
    printbuf(buf, "_");
    type_append(buf, elem, false);
    elem = ast_sibling(elem);
  }
}

static const char* stringtab_buf(printbuf_t* buf)
{
  const char* r = stringtab(buf->m);
  printbuf_free(buf);
  return r;
}

static const char* stringtab_two(const char* a, const char* b)
{
  if(a == NULL)
    a = "_";

  assert(b != NULL);
  printbuf_t* buf = printbuf_new();
  printbuf(buf, "%s_%s", a, b);
  return stringtab_buf(buf);
}

const char* genname_type(ast_t* ast)
{
  // package_Type[_Arg1_Arg2]
  printbuf_t* buf = printbuf_new();
  type_append(buf, ast, true);
  return stringtab_buf(buf);
}

const char* genname_type_and_cap(ast_t* ast)
{
  // package_Type[_Arg1_Arg2]_cap
  printbuf_t* buf = printbuf_new();
  type_append(buf, ast, false);
  return stringtab_buf(buf);
}

const char* genname_alloc(const char* type)
{
  return stringtab_two(type, "Alloc");
}

const char* genname_traitlist(const char* type)
{
  return stringtab_two(type, "Traits");
}

const char* genname_fieldlist(const char* type)
{
  return stringtab_two(type, "Fields");
}

const char* genname_trace(const char* type)
{
  return stringtab_two(type, "Trace");
}

const char* genname_serialise_trace(const char* type)
{
  return stringtab_two(type, "SerialiseTrace");
}

const char* genname_serialise(const char* type)
{
  return stringtab_two(type, "Serialise");
}

const char* genname_deserialise(const char* type)
{
  return stringtab_two(type, "Deserialise");
}

const char* genname_dispatch(const char* type)
{
  return stringtab_two(type, "Dispatch");
}

const char* genname_descriptor(const char* type)
{
  return stringtab_two(type, "Desc");
}

const char* genname_instance(const char* type)
{
  return stringtab_two(type, "Inst");
}

const char* genname_fun(token_id cap, const char* name, ast_t* typeargs)
{
  // cap_name[_Arg1_Arg2]
  printbuf_t* buf = printbuf_new();
  printbuf(buf, "%s_%s", lexer_print(cap), name);
  types_append(buf, typeargs);
  return stringtab_buf(buf);
}

const char* genname_be(const char* name)
{
  return stringtab_two(name, "_send");
}

const char* genname_box(const char* name)
{
  return stringtab_two(name, "Box");
}

const char* genname_unbox(const char* name)
{
  return stringtab_two(name, "Unbox");
}
