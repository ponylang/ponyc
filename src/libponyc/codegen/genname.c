#include "genname.h"
#include "../pkg/package.h"
#include "../type/typealias.h"
#include "../ast/stringtab.h"
#include "../ast/lexer.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <string.h>

static void types_append(printbuf_t* buf, ast_t* elements);

// Recursive aliases unfold to bodies that contain the same alias again.
// type_append walks structural type AST, so without protection it loops
// when it crosses a recursive alias. Track aliases currently being
// expanded; if we'd re-enter one, emit a placeholder and stop. The cap
// is a soft limit — exceeding it falls back to placeholder emission
// rather than crashing, even though real programs are nowhere near it.
#define MAX_ALIAS_DEPTH 64
static __pony_thread_local ast_t* unfold_stack[MAX_ALIAS_DEPTH];
static __pony_thread_local size_t unfold_depth;

static bool unfold_seen(ast_t* def)
{
  for(size_t i = 0; i < unfold_depth; i++)
  {
    if(unfold_stack[i] == def)
      return true;
  }
  return false;
}

static void type_append(printbuf_t* buf, ast_t* type, bool first)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    {
      // u3_Arg1_Arg2_Arg3
      printbuf(buf, "u%d", ast_childcount(type));
      types_append(buf, type);
      return;
    }

    case TK_ISECTTYPE:
    {
      // i3_Arg1_Arg2_Arg3
      printbuf(buf, "i%d", ast_childcount(type));
      types_append(buf, type);
      return;
    }

    case TK_TUPLETYPE:
    {
      // t3_Arg1_Arg2_Arg3
      printbuf(buf, "t%d", ast_childcount(type));
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

    case TK_TYPEALIASREF:
    {
      ast_t* def = (ast_t*)ast_data(type);

      if(unfold_seen(def) || unfold_depth >= MAX_ALIAS_DEPTH)
      {
        // Either we're re-entering a recursive alias (the def is on
        // the stack) or the chain is deeper than the soft cap. Either
        // way, emit the alias name as a terminator. For recursive
        // aliases, every encounter of the same alias produces the
        // same suffix, so two structurally identical types still
        // hash the same. For very deep non-recursive chains, the
        // truncation could in principle conflate two distinct types,
        // but the cap is far above any realistic nesting depth.
        AST_GET_CHILDREN(type, id);
        printbuf(buf, "%s", ast_name(id));
        return;
      }

      unfold_stack[unfold_depth++] = def;

      ast_t* unfolded = typealias_unfold(type);
      if(unfolded == NULL)
      {
        // typealias_unfold returns NULL only when reify fails (which
        // is caught at typecheck) or when the defensive depth cap
        // triggers (which means a legality-pass bug let an
        // alias-only cycle through). At codegen, neither should
        // happen. Fall back to emitting the alias name as a
        // terminator so the mangled name is well-formed even if a
        // future bug regresses; the asserted invariant is loud
        // enough in debug builds to catch it.
        pony_assert(false);
        AST_GET_CHILDREN(type, id);
        printbuf(buf, "%s", ast_name(id));
        unfold_depth--;
        return;
      }

      type_append(buf, unfolded, first);
      ast_free_unattached(unfolded);

      unfold_depth--;
      return;
    }

    default: {}
  }

  pony_assert(0);
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

  pony_assert(b != NULL);
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

const char* genname_traitmap(const char* type)
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

#if defined(USE_RUNTIME_TRACING)
const char* genname_get_behavior_name(const char* type)
{
  return stringtab_two(type, "GetBehaviourName");
}

const char* genname_behavior_name(const char* type, const char* name)
{
  printbuf_t* buf = printbuf_new();
  printbuf(buf, "%s.%s", type, name);
  return stringtab_buf(buf);
}
#endif

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
  if(cap == TK_AT)
    printbuf(buf, "%s", name);
  else
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

const char* genname_unsafe(const char* name)
{
  return stringtab_two(name, "unsafe");
}

const char* genname_program_fn(const char* program, const char* name)
{
  return stringtab_two(program, name);
}

const char* genname_type_with_id(const char* type, uint64_t type_id)
{
  printbuf_t* buf = printbuf_new();
  printbuf(buf, "%s_%" PRIu64, type, type_id);
  return stringtab_buf(buf);
}
