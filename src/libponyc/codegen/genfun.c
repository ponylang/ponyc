#include "genfun.h"
#include "gentype.h"
#include "genname.h"
#include "gencall.h"
#include "gentrace.h"
#include "gencontrol.h"
#include "genexpr.h"
#include "../pass/names.h"
#include "../type/assemble.h"
#include "../type/subtype.h"
#include "../type/reify.h"
#include "../type/lookup.h"
#include "../../libponyrt/ds/fun.h"
#include "../../libponyrt/mem/pool.h"
#include <string.h>
#include <assert.h>

static void name_param(compile_t* c, LLVMValueRef func, const char* name,
  ast_t* type, int index)
{
  gentype_t g;
  gentype(c, type, &g);

  LLVMValueRef param = LLVMGetParam(func, index);
  LLVMSetValueName(param, name);

  LLVMValueRef value = LLVMBuildAlloca(c->builder, g.use_type, name);
  LLVMBuildStore(c->builder, param, value);
  codegen_setlocal(c, name, value);
}

static void name_params(compile_t* c, ast_t* type, ast_t* params,
  LLVMValueRef func)
{
  int count = 0;

  // Name the receiver 'this'.
  name_param(c, func, stringtab("this"), type, count++);

  // Name each parameter.
  ast_t* param = ast_child(params);

  while(param != NULL)
  {
    AST_GET_CHILDREN(param, id, type);
    name_param(c, func, ast_name(id), type, count++);
    param = ast_sibling(param);
  }
}

static ast_t* get_fun(gentype_t* g, const char* name, ast_t* typeargs)
{
  ast_t* this_type = set_cap_and_ephemeral(g->ast, TK_REF, TK_NONE);
  ast_t* fun = lookup_try(NULL, NULL, this_type, name);
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

static LLVMTypeRef get_signature(compile_t* c, gentype_t* g, ast_t* fun)
{
  // Get a type for the result.
  ast_t* rtype = ast_childidx(fun, 4);
  gentype_t rtype_g;

  if(!gentype(c, rtype, &rtype_g))
  {
    ast_error(rtype, "couldn't generate result type");
    return NULL;
  }

  // Count the parameters, including the receiver.
  ast_t* params = ast_childidx(fun, 3);
  size_t count = ast_childcount(params) + 1;

  size_t buf_size = count * sizeof(LLVMTypeRef);
  LLVMTypeRef* tparams = (LLVMTypeRef*)ponyint_pool_alloc_size(buf_size);
  count = 0;

  // Get a type for the receiver.
  tparams[count++] = g->use_type;

  // Get a type for each parameter.
  ast_t* param = ast_child(params);

  while(param != NULL)
  {
    ast_t* ptype = ast_childidx(param, 1);
    gentype_t ptype_g;

    if(!gentype(c, ptype, &ptype_g))
    {
      ast_error(ptype, "couldn't generate parameter type");
      ponyint_pool_free_size(buf_size, tparams);
      return NULL;
    }

    tparams[count++] = ptype_g.use_type;
    param = ast_sibling(param);
  }

  LLVMTypeRef result = rtype_g.use_type;

  // Generate the function type.
  LLVMTypeRef r = LLVMFunctionType(result, tparams, (int)count, false);

  ponyint_pool_free_size(buf_size, tparams);
  return r;
}

static LLVMValueRef get_prototype(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs, ast_t* fun)
{
  // Behaviours and actor constructors also have sender functions.
  bool sender = false;

  switch(ast_id(fun))
  {
    case TK_NEW:
      sender = g->underlying == TK_ACTOR;
      break;

    case TK_BE:
      sender = true;
      break;

    default: {}
  }

  // Get a fully qualified name: starts with the type name, followed by the
  // type arguments, followed by the function name, followed by the function
  // level type arguments.
  const char* funname = genname_fun(g->type_name, name, typeargs);

  // If the function already exists, just return it.
  LLVMValueRef func = LLVMGetNamedFunction(c->module, funname);

  if(func != NULL)
    return func;

  LLVMTypeRef ftype = get_signature(c, g, fun);

  if(ftype == NULL)
    return NULL;

  // If the function exists now, just return it.
  func = LLVMGetNamedFunction(c->module, funname);

  if(func != NULL)
    return func;

  if(sender)
  {
    // Generate the sender prototype.
    const char* be_name = genname_be(funname);
    func = codegen_addfun(c, be_name, ftype);

    // Change the return type to void for the handler.
    size_t count = LLVMCountParamTypes(ftype);
    size_t buf_size = count * sizeof(LLVMTypeRef);
    LLVMTypeRef* tparams = (LLVMTypeRef*)ponyint_pool_alloc_size(buf_size);
    LLVMGetParamTypes(ftype, tparams);

    ftype = LLVMFunctionType(c->void_type, tparams, (int)count, false);
    ponyint_pool_free_size(buf_size, tparams);
  }

  // Generate the function prototype.
  return codegen_addfun(c, funname, ftype);
}

static void genfun_dwarf(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs, ast_t* fun)
{
  if(!codegen_hassource(c))
    return;

  // Get the function.
  const char* funname = genname_fun(g->type_name, name, typeargs);
  LLVMValueRef func = LLVMGetNamedFunction(c->module, funname);
  assert(func != NULL);

  // Count the parameters, including the receiver.
  ast_t* params = ast_childidx(fun, 3);
  size_t count = ast_childcount(params) + 1;

  size_t buf_size = (count + 1) * sizeof(const char*);
  const char** pnames = (const char**)ponyint_pool_alloc_size(buf_size);
  count = 0;

  // Return value type name and receiver type name.
  pnames[count++] = genname_type(ast_childidx(fun, 4));
  pnames[count++] = g->type_name;

  // Get a type name for each parameter.
  ast_t* param = ast_child(params);

  while(param != NULL)
  {
    ast_t* ptype = ast_childidx(param, 1);
    pnames[count++] = genname_type(ptype);
    param = ast_sibling(param);
  }

  // Dwarf the method type
  dwarf_method(&c->dwarf, fun, name, funname, pnames, count, func);

  // Dwarf the receiver pointer.
  LLVMBasicBlockRef entry = LLVMGetEntryBasicBlock(codegen_fun(c));
  LLVMValueRef argument = codegen_getlocal(c, stringtab("this"));

  dwarf_this(&c->dwarf, fun, g->type_name, entry, argument);

  // Dwarf locals for parameters
  param = ast_child(params);
  size_t index = 1;

  while(param != NULL)
  {
    argument = codegen_getlocal(c, ast_name(ast_child(param)));
    dwarf_parameter(&c->dwarf, param, pnames[index + 1], entry, argument,
      index);
    param = ast_sibling(param);
    index++;
  }

  ponyint_pool_free_size(buf_size, pnames);
}

static void genfun_dwarf_return(compile_t* c, ast_t* body)
{
  ast_t* last = ast_childlast(body);
  dwarf_location(&c->dwarf, last);
}

static LLVMValueRef get_sender(compile_t* c, gentype_t* g, const char* name,
  ast_t* typeargs)
{
  const char* fun_name = genname_fun(g->type_name, name, typeargs);
  const char* be_name = genname_be(fun_name);
  return LLVMGetNamedFunction(c->module, be_name);
}

static LLVMTypeRef send_message(compile_t* c, ast_t* fun, LLVMValueRef to,
  LLVMValueRef func, uint32_t index)
{
  // Get the parameter types.
  LLVMTypeRef f_type = LLVMGetElementType(LLVMTypeOf(func));
  int count = LLVMCountParamTypes(f_type) + 2;

  size_t buf_size = count * sizeof(LLVMTypeRef);
  LLVMTypeRef* f_params = (LLVMTypeRef*)ponyint_pool_alloc_size(buf_size);
  LLVMGetParamTypes(f_type, &f_params[2]);

  // The first one becomes the message size, the second the message ID.
  f_params[0] = c->i32;
  f_params[1] = c->i32;
  f_params[2] = c->void_ptr;
  LLVMTypeRef msg_type = LLVMStructTypeInContext(c->context, f_params, count,
    false);
  LLVMTypeRef msg_type_ptr = LLVMPointerType(msg_type, 0);
  ponyint_pool_free_size(buf_size, f_params);

  // Allocate the message, setting its size and ID.
  size_t msg_size = (size_t)LLVMABISizeOfType(c->target_data, msg_type);
  LLVMValueRef args[3];

  args[0] = LLVMConstInt(c->i32, ponyint_pool_index(msg_size), false);
  args[1] = LLVMConstInt(c->i32, index, false);
  LLVMValueRef msg = gencall_runtime(c, "pony_alloc_msg", args, 2, "");
  LLVMValueRef msg_ptr = LLVMBuildBitCast(c->builder, msg, msg_type_ptr, "");

  // Trace while populating the message contents.
  LLVMValueRef ctx = codegen_ctx(c);
  LLVMValueRef start_trace = gencall_runtime(c, "pony_gc_send", &ctx, 1, "");
  ast_t* params = ast_childidx(fun, 3);
  ast_t* param = ast_child(params);
  bool need_trace = false;

  for(int i = 3; i < count; i++)
  {
    LLVMValueRef arg = LLVMGetParam(func, i - 2);
    LLVMValueRef arg_ptr = LLVMBuildStructGEP(c->builder, msg_ptr, i, "");
    LLVMBuildStore(c->builder, arg, arg_ptr);

    need_trace |= gentrace(c, ctx, arg, ast_type(param));
    param = ast_sibling(param);
  }

  if(need_trace)
  {
    gencall_runtime(c, "pony_send_done", &ctx, 1, "");
  } else {
    LLVMInstructionEraseFromParent(start_trace);
  }

  // Send the message.
  args[0] = ctx;
  args[1] = LLVMBuildBitCast(c->builder, to, c->object_ptr, "");
  args[2] = msg;
  gencall_runtime(c, "pony_sendv", args, 3, "");

  // Return the type of the message.
  return msg_type_ptr;
}

static void add_dispatch_case(compile_t* c, gentype_t* g, ast_t* fun,
  uint32_t index, LLVMValueRef handler, LLVMTypeRef type)
{
  // Add a case to the dispatch function to handle this message.
  codegen_startfun(c, g->dispatch_fn, false);
  LLVMBasicBlockRef block = codegen_block(c, "handler");
  LLVMValueRef id = LLVMConstInt(c->i32, index, false);
  LLVMAddCase(g->dispatch_switch, id, block);

  // Destructure the message.
  LLVMPositionBuilderAtEnd(c->builder, block);
  LLVMValueRef ctx = LLVMGetParam(g->dispatch_fn, 0);
  LLVMValueRef this_ptr = LLVMGetParam(g->dispatch_fn, 1);
  LLVMValueRef msg = LLVMBuildBitCast(c->builder,
    LLVMGetParam(g->dispatch_fn, 2), type, "");

  int count = LLVMCountParams(handler);
  size_t buf_size = count * sizeof(LLVMValueRef);
  LLVMValueRef* args = (LLVMValueRef*)ponyint_pool_alloc_size(buf_size);
  args[0] = LLVMBuildBitCast(c->builder, this_ptr, g->use_type, "");

  // Trace the message.
  LLVMValueRef start_trace = gencall_runtime(c, "pony_gc_recv", &ctx, 1, "");
  ast_t* params = ast_childidx(fun, 3);
  ast_t* param = ast_child(params);
  bool need_trace = false;

  for(int i = 1; i < count; i++)
  {
    LLVMValueRef field = LLVMBuildStructGEP(c->builder, msg, i + 2, "");
    args[i] = LLVMBuildLoad(c->builder, field, "");

    need_trace |= gentrace(c, ctx, args[i], ast_type(param));
    param = ast_sibling(param);
  }

  if(need_trace)
  {
    gencall_runtime(c, "pony_recv_done", &ctx, 1, "");
  } else {
    LLVMInstructionEraseFromParent(start_trace);
  }

  // Call the handler.
  codegen_call(c, handler, args, count);
  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);
  ponyint_pool_free_size(buf_size, args);
}

LLVMTypeRef genfun_sig(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs)
{
  // If the function already exists, return its type.
  const char* funname = genname_fun(g->type_name, name, typeargs);
  LLVMValueRef func = LLVMGetNamedFunction(c->module, funname);

  if(func != NULL)
    return LLVMGetElementType(LLVMTypeOf(func));

  ast_t* fun = get_fun(g, name, typeargs);
  LLVMTypeRef type = get_signature(c, g, fun);
  ast_free_unattached(fun);
  return type;
}

LLVMValueRef genfun_proto(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs)
{
  ast_t* fun = get_fun(g, name, typeargs);
  LLVMValueRef func = get_prototype(c, g, name, typeargs, fun);

  // Disable debugloc on calls to methods that have no debug info.
  if(!ast_debug(fun))
    dwarf_location(&c->dwarf, NULL);

  switch(ast_id(fun))
  {
    case TK_NEW:
    case TK_BE:
      if(g->underlying == TK_ACTOR)
      {
        const char* fun_name = genname_fun(g->type_name, name, typeargs);
        const char* be_name = genname_be(fun_name);
        func = LLVMGetNamedFunction(c->module, be_name);
      }
      break;

    default: {}
  }

  ast_free_unattached(fun);
  return func;
}

static LLVMValueRef genfun_fun(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs)
{
  ast_t* fun = get_fun(g, name, typeargs);
  LLVMValueRef func = get_prototype(c, g, name, typeargs, fun);

  if(func == NULL)
  {
    ast_free_unattached(fun);
    return NULL;
  }

  if(LLVMCountBasicBlocks(func) != 0)
  {
    ast_free_unattached(fun);
    return func;
  }

  if(!strcmp(name, "_final"))
    LLVMSetFunctionCallConv(func, LLVMCCallConv);

  codegen_startfun(c, func, ast_debug(fun));
  name_params(c, g->ast, ast_childidx(fun, 3), func);
  genfun_dwarf(c, g, name, typeargs, fun);

  ast_t* body = ast_childidx(fun, 6);
  LLVMValueRef value = gen_expr(c, body);

  if(value == NULL)
  {
    ast_free_unattached(fun);
    return NULL;
  } else if(value != GEN_NOVALUE) {
    genfun_dwarf_return(c, body);

    LLVMTypeRef f_type = LLVMGetElementType(LLVMTypeOf(func));
    LLVMTypeRef r_type = LLVMGetReturnType(f_type);

    // If the result type is known to be a tuple, do the correct assignment
    // cast even if the body type is not a tuple.
    ast_t* body_type = ast_type(body);
    ast_t* result_type = ast_childidx(fun, 4);

    if(ast_id(result_type) == TK_TUPLETYPE)
      body_type = result_type;

    LLVMValueRef ret = gen_assign_cast(c, r_type, value, body_type);

    if(ret == NULL)
    {
      ast_free_unattached(fun);
      return NULL;
    }

    LLVMBuildRet(c->builder, ret);
  }

  codegen_finishfun(c);
  ast_free_unattached(fun);

  return func;
}

static LLVMValueRef genfun_be(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs)
{
  ast_t* fun = get_fun(g, name, typeargs);
  LLVMValueRef func = get_prototype(c, g, name, typeargs, fun);

  if(func == NULL)
  {
    ast_free_unattached(fun);
    return NULL;
  }

  codegen_startfun(c, func, ast_debug(fun));
  name_params(c, g->ast, ast_childidx(fun, 3), func);
  genfun_dwarf(c, g, name, typeargs, fun);

  ast_t* body = ast_childidx(fun, 6);
  LLVMValueRef value = gen_expr(c, body);

  if(value == NULL)
  {
    ast_free_unattached(fun);
    return NULL;
  } else if(value != GEN_NOVALUE) {
    genfun_dwarf_return(c, body);
    LLVMBuildRetVoid(c->builder);
  }

  codegen_finishfun(c);

  // Generate the sender.
  LLVMValueRef sender = get_sender(c, g, name, typeargs);
  codegen_startfun(c, sender, false);
  LLVMValueRef this_ptr = LLVMGetParam(sender, 0);

  // Send the arguments in a message to 'this'.
  uint32_t index = genfun_vtable_index(c, g, name, typeargs);
  LLVMTypeRef msg_type_ptr = send_message(c, fun, this_ptr, sender, index);

  // Return 'this'.
  LLVMBuildRet(c->builder, this_ptr);
  codegen_finishfun(c);

  // Add the dispatch case.
  add_dispatch_case(c, g, fun, index, func, msg_type_ptr);
  ast_free_unattached(fun);

  return func;
}

static LLVMValueRef genfun_new(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs)
{
  ast_t* fun = get_fun(g, name, typeargs);
  LLVMValueRef func = get_prototype(c, g, name, typeargs, fun);

  if(func == NULL)
  {
    ast_free_unattached(fun);
    return NULL;
  }

  if(LLVMCountBasicBlocks(func) != 0)
  {
    ast_free_unattached(fun);
    return func;
  }

  codegen_startfun(c, func, ast_debug(fun));
  name_params(c, g->ast, ast_childidx(fun, 3), func);
  genfun_dwarf(c, g, name, typeargs, fun);

  ast_t* body = ast_childidx(fun, 6);
  LLVMValueRef value = gen_expr(c, body);

  if(value == NULL)
  {
    ast_free_unattached(fun);
    return NULL;
  }

  genfun_dwarf_return(c, body);

  // Return 'this'.
  if(g->primitive == NULL)
    value = LLVMGetParam(func, 0);

  LLVMBuildRet(c->builder, value);
  codegen_finishfun(c);

  ast_free_unattached(fun);
  return func;
}

static LLVMValueRef genfun_newbe(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs)
{
  ast_t* fun = get_fun(g, name, typeargs);
  LLVMValueRef func = get_prototype(c, g, name, typeargs, fun);

  if(func == NULL)
  {
    ast_free_unattached(fun);
    return NULL;
  }

  codegen_startfun(c, func, ast_debug(fun));
  name_params(c, g->ast, ast_childidx(fun, 3), func);
  genfun_dwarf(c, g, name, typeargs, fun);

  ast_t* body = ast_childidx(fun, 6);
  LLVMValueRef value = gen_expr(c, body);

  if(value == NULL)
  {
    ast_free_unattached(fun);
    return NULL;
  }

  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);

  // Generate the sender.
  LLVMValueRef sender = get_sender(c, g, name, typeargs);
  codegen_startfun(c, sender, false);
  LLVMValueRef this_ptr = LLVMGetParam(sender, 0);

  // Send the arguments in a message to 'this'.
  uint32_t index = genfun_vtable_index(c, g, name, typeargs);
  LLVMTypeRef msg_type_ptr = send_message(c, fun, this_ptr, sender, index);

  genfun_dwarf_return(c, body);

  // Return 'this'.
  LLVMBuildRet(c->builder, this_ptr);
  codegen_finishfun(c);

  // Add the dispatch case.
  add_dispatch_case(c, g, fun, index, func, msg_type_ptr);
  ast_free_unattached(fun);

  return func;
}

static bool genfun_allocator(compile_t* c, gentype_t* g)
{
  // No allocator for primitive types or pointers.
  if((g->primitive != NULL) || is_pointer(g->ast) || is_maybe(g->ast))
    return true;

  const char* funname = genname_fun(g->type_name, "Alloc", NULL);
  LLVMTypeRef ftype = LLVMFunctionType(g->use_type, NULL, 0, false);
  LLVMValueRef fun = codegen_addfun(c, funname, ftype);
  codegen_startfun(c, fun, false);

  LLVMValueRef result;

  switch(g->underlying)
  {
    case TK_PRIMITIVE:
    case TK_STRUCT:
    case TK_CLASS:
      // Allocate the object or return the global instance.
      result = gencall_alloc(c, g);
      break;

    case TK_ACTOR:
      // Allocate the actor.
      result = gencall_create(c, g);
      break;

    default:
      assert(0);
      return false;
  }

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
  return true;
}

bool genfun_methods(compile_t* c, gentype_t* g)
{
  reachable_type_t* t = reach_type(c->reachable, g->type_name);

  size_t i = HASHMAP_BEGIN;
  reachable_method_name_t* n;

  while((n = reachable_method_names_next(&t->methods, &i)) != NULL)
  {
    size_t j = HASHMAP_BEGIN;
    reachable_method_t* m;
    LLVMValueRef fun;

    while((m = reachable_methods_next(&n->r_methods, &j)) != NULL)
    {
      switch(ast_id(m->r_fun))
      {
        case TK_NEW:
          if(g->underlying == TK_ACTOR)
            fun = genfun_newbe(c, g, n->name, m->typeargs);
          else
            fun = genfun_new(c, g, n->name, m->typeargs);
          break;

        case TK_BE:
          fun = genfun_be(c, g, n->name, m->typeargs);
          break;

        case TK_FUN:
          fun = genfun_fun(c, g, n->name, m->typeargs);
          break;

        default:
          fun = NULL;
          break;
      }

      if(fun == NULL)
        return false;
    }
  }

  if(!genfun_allocator(c, g))
    return false;

  return true;
}

uint32_t genfun_vtable_size(compile_t* c, gentype_t* g)
{
  reachable_type_t* t = reach_type(c->reachable, g->type_name);

  if(t == NULL)
    return 0;

  return t->vtable_size;
}

static uint32_t vtable_index(compile_t* c, const char* type_name,
  const char* name, ast_t* typeargs)
{
  reachable_type_t* t = reach_type(c->reachable, type_name);

  if(t == NULL)
    return -1;

  reachable_method_name_t* n = reach_method_name(t, name);

  if(n == NULL)
    return -1;

  if(typeargs != NULL)
    name = genname_fun(NULL, name, typeargs);

  reachable_method_t* m = reach_method(n, name);

  if(m == NULL)
    return -1;

  assert(m->vtable_index != (uint32_t)-1);
  return m->vtable_index;
}

uint32_t genfun_vtable_index(compile_t* c, gentype_t* g, const char* name,
  ast_t* typeargs)
{
  switch(ast_id(g->ast))
  {
    case TK_NOMINAL:
      return vtable_index(c, g->type_name, name, typeargs);

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      ast_t* child = ast_child(g->ast);

      while(child != NULL)
      {
        const char* type_name = genname_type(child);
        uint32_t index = vtable_index(c, type_name, name, typeargs);

        if(index != (uint32_t)-1)
          return index;

        child = ast_sibling(child);
      }

      return -1;
    }

    default: {}
  }

  return -1;
}
