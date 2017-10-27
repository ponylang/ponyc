#include "genfun.h"
#include "genname.h"
#include "gencall.h"
#include "gentrace.h"
#include "gencontrol.h"
#include "genexpr.h"
#include "genreference.h"
#include "../pass/names.h"
#include "../type/assemble.h"
#include "../type/subtype.h"
#include "../type/reify.h"
#include "../type/lookup.h"
#include "../../libponyrt/ds/fun.h"
#include "../../libponyrt/mem/pool.h"
#include "../../libponyrt/mem/heap.h"
#include "ponyassert.h"
#include <string.h>

static void compile_method_free(void* p)
{
  POOL_FREE(compile_method_t, p);
}

static void name_param(compile_t* c, reach_type_t* t, reach_method_t* m,
  LLVMValueRef func, const char* name, unsigned index, size_t line, size_t pos)
{
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  compile_method_t* c_m = (compile_method_t*)m->c_method;

  LLVMValueRef value = LLVMGetParam(func, index);
  LLVMSetValueName(value, name);

  LLVMValueRef alloc = LLVMBuildAlloca(c->builder, c_t->mem_type, name);
  value = gen_assign_cast(c, c_t->mem_type, value, t->ast_cap);
  LLVMBuildStore(c->builder, value, alloc);
  codegen_setlocal(c, name, alloc);

  LLVMMetadataRef info;

  if(index == 0)
  {
    info = LLVMDIBuilderCreateArtificialVariable(c->di,
      c_m->di_method, name, index + 1, c_m->di_file,
      (unsigned)ast_line(m->r_fun), c_t->di_type);
  } else {
    info = LLVMDIBuilderCreateParameterVariable(c->di,
      c_m->di_method, name, index + 1, c_m->di_file,
      (unsigned)ast_line(m->r_fun), c_t->di_type);
  }

  LLVMMetadataRef expr = LLVMDIBuilderCreateExpression(c->di, NULL, 0);

  LLVMDIBuilderInsertDeclare(c->di, alloc, info, expr,
    (unsigned)line, (unsigned)pos, c_m->di_method,
    LLVMGetInsertBlock(c->builder));
}

static void name_params(compile_t* c, reach_type_t* t, reach_method_t* m,
  ast_t* params, LLVMValueRef func)
{
  unsigned offset = 0;

  if(m->cap != TK_AT)
  {
    // Name the receiver 'this'.
    name_param(c, t, m, func, c->str_this, 0, ast_line(params), ast_pos(params));
    offset = 1;
  }

  // Name each parameter.
  ast_t* param = ast_child(params);

  for(size_t i = 0; i < m->param_count; i++)
  {
    name_param(c, m->params[i].type, m, func, ast_name(ast_child(param)),
      (unsigned)i + offset, ast_line(param), ast_pos(param));
    param = ast_sibling(param);
  }
}

static void make_signature(compile_t* c, reach_type_t* t,
  reach_method_name_t* n, reach_method_t* m, bool message_type)
{
  // Count the parameters, including the receiver if the method isn't bare.
  size_t count = m->param_count;
  size_t offset = 0;
  if(m->cap != TK_AT)
  {
    count++;
    offset++;
  }

  size_t tparam_size = count * sizeof(LLVMTypeRef);

  if(message_type)
    tparam_size += tparam_size + (2 * sizeof(LLVMTypeRef));

  LLVMTypeRef* tparams = (LLVMTypeRef*)ponyint_pool_alloc_size(tparam_size);
  LLVMTypeRef* mparams = NULL;

  if(message_type)
    mparams = &tparams[count];

  bool bare_void = false;
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  compile_method_t* c_m = (compile_method_t*)m->c_method;

  if(m->cap == TK_AT)
  {
    bare_void = is_none(m->result->ast);
  } else {
    // Get a type for the receiver.
    tparams[0] = c_t->use_type;
  }

  // Get a type for each parameter.
  for(size_t i = 0; i < m->param_count; i++)
  {
    compile_type_t* p_c_t = (compile_type_t*)m->params[i].type->c_type;
    tparams[i + offset] = p_c_t->use_type;

    if(message_type)
      mparams[i + offset + 2] = p_c_t->mem_type;
  }

  // Generate the function type.
  // Bare methods returning None return void to maintain compatibility with C.
  // Class constructors return void to avoid clobbering nocapture information.
  if(bare_void || (n->name == c->str__final) ||
    ((ast_id(m->r_fun) == TK_NEW) && (t->underlying == TK_CLASS)))
    c_m->func_type = LLVMFunctionType(c->void_type, tparams, (int)count, false);
  else
    c_m->func_type = LLVMFunctionType(
      ((compile_type_t*)m->result->c_type)->use_type, tparams, (int)count,
      false);

  if(message_type)
  {
    mparams[0] = c->i32;
    mparams[1] = c->i32;
    mparams[2] = c->void_ptr;

    c_m->msg_type = LLVMStructTypeInContext(c->context, mparams, (int)count + 2,
      false);
  }

  ponyint_pool_free_size(tparam_size, tparams);
}

static void make_function_debug(compile_t* c, reach_type_t* t,
  reach_method_t* m, LLVMValueRef func)
{
  AST_GET_CHILDREN(m->r_fun, cap, id, typeparams, params, result, can_error,
    body);

  // Count the parameters, including the receiver and the result.
  size_t count = m->param_count + 1;
  size_t offset = 1;
  if(m->cap != TK_AT)
  {
    count++;
    offset++;
  }

  size_t md_size = count * sizeof(reach_type_t*);
  LLVMMetadataRef* md = (LLVMMetadataRef*)ponyint_pool_alloc_size(md_size);
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  compile_method_t* c_m = (compile_method_t*)m->c_method;

  md[0] = ((compile_type_t*)m->result->c_type)->di_type;
  if(m->cap != TK_AT)
    md[1] = c_t->di_type;

  for(size_t i = 0; i < m->param_count; i++)
    md[i + offset] = ((compile_type_t*)m->params[i].type->c_type)->di_type;

  c_m->di_file = c_t->di_file;

  LLVMMetadataRef type_array = LLVMDIBuilderGetOrCreateTypeArray(c->di,
    md, count);

  LLVMMetadataRef subroutine = LLVMDIBuilderCreateSubroutineType(c->di,
    c_m->di_file, type_array);

  LLVMMetadataRef scope;

  if(c_t->di_type_embed != NULL)
    scope = c_t->di_type_embed;
  else
    scope = c_t->di_type;

  c_m->di_method = LLVMDIBuilderCreateMethod(c->di, scope, ast_name(id),
    m->full_name, c_m->di_file, (unsigned)ast_line(m->r_fun), subroutine, func,
    c->opt->release);

  ponyint_pool_free_size(md_size, md);
}

static void make_prototype(compile_t* c, reach_type_t* t,
  reach_method_name_t* n, reach_method_t* m)
{
  if(m->intrinsic)
    return;

  // Behaviours and actor constructors also have handler functions.
  bool handler = false;
  bool is_trait = false;

  switch(ast_id(m->r_fun))
  {
    case TK_NEW:
      handler = t->underlying == TK_ACTOR;
      break;

    case TK_BE:
      handler = true;
      break;

    default: {}
  }

  switch(t->underlying)
  {
    case TK_PRIMITIVE:
    case TK_STRUCT:
    case TK_CLASS:
    case TK_ACTOR:
      break;

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_INTERFACE:
    case TK_TRAIT:
      is_trait = true;
      break;

    default:
      pony_assert(0);
      return;
  }

  make_signature(c, t, n, m, handler || is_trait);

  if(is_trait)
    return;

  compile_method_t* c_m = (compile_method_t*)m->c_method;

  if(!handler)
  {
    // Generate the function prototype.
    c_m->func = codegen_addfun(c, m->full_name, c_m->func_type, true);
    genfun_param_attrs(c, t, m, c_m->func);
    make_function_debug(c, t, m, c_m->func);
  } else {
    size_t count = LLVMCountParamTypes(c_m->func_type);
    size_t buf_size = count * sizeof(LLVMTypeRef);
    LLVMTypeRef* tparams = (LLVMTypeRef*)ponyint_pool_alloc_size(buf_size);
    LLVMGetParamTypes(c_m->func_type, tparams);

    // Generate the sender prototype.
    const char* sender_name = genname_be(m->full_name);
    c_m->func = codegen_addfun(c, sender_name, c_m->func_type, true);
    genfun_param_attrs(c, t, m, c_m->func);

    // If the method is a forwarding mangling, we don't need the handler.
    if(!m->forwarding)
    {
      // Change the return type to void for the handler.
      LLVMTypeRef handler_type = LLVMFunctionType(c->void_type, tparams,
        (int)count, false);

      // Generate the handler prototype.
      c_m->func_handler = codegen_addfun(c, m->full_name, handler_type, true);
      genfun_param_attrs(c, t, m, c_m->func_handler);
      make_function_debug(c, t, m, c_m->func_handler);
    }

    ponyint_pool_free_size(buf_size, tparams);
  }

  compile_type_t* c_t = (compile_type_t*)t->c_type;

  if(n->name == c->str__final)
  {
    // Store the finaliser and use the C calling convention and an external
    // linkage.
    pony_assert(c_t->final_fn == NULL);
    c_t->final_fn = c_m->func;
    LLVMSetFunctionCallConv(c_m->func, LLVMCCallConv);
    LLVMSetLinkage(c_m->func, LLVMExternalLinkage);
  } else if(n->name == c->str__serialise_space) {
    c_t->custom_serialise_space_fn = c_m->func;
    LLVMSetFunctionCallConv(c_m->func, LLVMCCallConv);
    LLVMSetLinkage(c_m->func, LLVMExternalLinkage);
  } else if(n->name == c->str__serialise) {
    c_t->custom_serialise_fn = c_m->func;
  } else if(n->name == c->str__deserialise) {
    c_t->custom_deserialise_fn = c_m->func;
    LLVMSetFunctionCallConv(c_m->func, LLVMCCallConv);
    LLVMSetLinkage(c_m->func, LLVMExternalLinkage);
  }

  if(n->cap == TK_AT)
  {
    LLVMSetFunctionCallConv(c_m->func, LLVMCCallConv);
    LLVMSetLinkage(c_m->func, LLVMExternalLinkage);
    LLVMSetUnnamedAddr(c_m->func, false);

    if(t->bare_method == m)
    {
      pony_assert(c_t->instance == NULL);
      c_t->instance = LLVMConstBitCast(c_m->func, c->void_ptr);
    }
  }
}

static void add_dispatch_case(compile_t* c, reach_type_t* t, ast_t* params,
  uint32_t index, LLVMValueRef handler, LLVMTypeRef fun_type,
  LLVMTypeRef msg_type)
{
  // Add a case to the dispatch function to handle this message.
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  codegen_startfun(c, c_t->dispatch_fn, NULL, NULL, false);
  LLVMBasicBlockRef block = codegen_block(c, "handler");
  LLVMValueRef id = LLVMConstInt(c->i32, index, false);
  LLVMAddCase(c_t->dispatch_switch, id, block);

  // Destructure the message.
  LLVMPositionBuilderAtEnd(c->builder, block);
  LLVMValueRef ctx = LLVMGetParam(c_t->dispatch_fn, 0);
  LLVMValueRef this_ptr = LLVMGetParam(c_t->dispatch_fn, 1);
  LLVMValueRef msg = LLVMBuildBitCast(c->builder,
    LLVMGetParam(c_t->dispatch_fn, 2), msg_type, "");

  int count = LLVMCountParamTypes(fun_type);
  size_t params_buf_size = count * sizeof(LLVMTypeRef);
  LLVMTypeRef* param_types =
    (LLVMTypeRef*)ponyint_pool_alloc_size(params_buf_size);
  LLVMGetParamTypes(fun_type, param_types);

  size_t args_buf_size = count * sizeof(LLVMValueRef);
  LLVMValueRef* args = (LLVMValueRef*)ponyint_pool_alloc_size(args_buf_size);
  args[0] = LLVMBuildBitCast(c->builder, this_ptr, c_t->use_type, "");

  ast_t* param = ast_child(params);

  for(int i = 1; i < count; i++)
  {
    LLVMValueRef field = LLVMBuildStructGEP(c->builder, msg, i + 2, "");
    args[i] = LLVMBuildLoad(c->builder, field, "");
    args[i] = gen_assign_cast(c, param_types[i], args[i], ast_type(param));
    param = ast_sibling(param);
  }

  // Trace the message.
  param = ast_child(params);
  bool need_trace = false;

  while(param != NULL)
  {
    ast_t* param_type = ast_type(param);
    if(gentrace_needed(c, param_type, param_type))
    {
      need_trace = true;
      break;
    }

    param = ast_sibling(param);
  }

  if(need_trace)
  {
    param = ast_child(params);
    gencall_runtime(c, "pony_gc_recv", &ctx, 1, "");

    for(int i = 1; i < count; i++)
    {
      ast_t* param_type = ast_type(param);
      gentrace(c, ctx, args[i], args[i], param_type, param_type);
      param = ast_sibling(param);
    }

    gencall_runtime(c, "pony_recv_done", &ctx, 1, "");
  }

  // Call the handler.
  codegen_call(c, handler, args, count, true);
  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);
  ponyint_pool_free_size(args_buf_size, args);
  ponyint_pool_free_size(params_buf_size, param_types);
}

static void call_embed_finalisers(compile_t* c, reach_type_t* t,
  ast_t* call_location, LLVMValueRef obj)
{
  uint32_t base = 0;
  if(t->underlying != TK_STRUCT)
    base++;

  if(t->underlying == TK_ACTOR)
    base++;

  for(uint32_t i = 0; i < t->field_count; i++)
  {
    reach_field_t* field = &t->fields[i];
    if(!field->embed)
      continue;

    LLVMValueRef final_fn = ((compile_type_t*)field->type->c_type)->final_fn;
    if(final_fn == NULL)
      continue;

    LLVMValueRef field_ref = LLVMBuildStructGEP(c->builder, obj, base + i, "");
    codegen_debugloc(c, call_location);
    LLVMBuildCall(c->builder, final_fn, &field_ref, 1, "");
    codegen_debugloc(c, NULL);
  }
}

static bool genfun_fun(compile_t* c, reach_type_t* t, reach_method_t* m)
{
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  compile_method_t* c_m = (compile_method_t*)m->c_method;
  pony_assert(c_m->func != NULL);

  AST_GET_CHILDREN(m->r_fun, cap, id, typeparams, params, result, can_error,
    body);

  codegen_startfun(c, c_m->func, c_m->di_file, c_m->di_method,
    ast_id(cap) == TK_AT);
  name_params(c, t, m, params, c_m->func);

  bool finaliser = c_m->func == c_t->final_fn;

  if(finaliser)
    call_embed_finalisers(c, t, body, gen_this(c, NULL));

  LLVMValueRef value = gen_expr(c, body);

  if(value == NULL)
    return false;

  if(value != GEN_NOVALUE)
  {
    if(finaliser || ((ast_id(cap) == TK_AT) && is_none(result)))
    {
      codegen_scope_lifetime_end(c);
      codegen_debugloc(c, ast_childlast(body));
      LLVMBuildRetVoid(c->builder);
    } else {
      LLVMTypeRef f_type = LLVMGetElementType(LLVMTypeOf(c_m->func));
      LLVMTypeRef r_type = LLVMGetReturnType(f_type);

      // If the result type is known to be a tuple, do the correct assignment
      // cast even if the body type is not a tuple.
      ast_t* body_type = ast_type(body);

      if((ast_id(result) == TK_TUPLETYPE) && (ast_id(body_type) != TK_TUPLETYPE))
        body_type = result;

      LLVMValueRef ret = gen_assign_cast(c, r_type, value, body_type);

      if(ret == NULL)
        return false;

      codegen_scope_lifetime_end(c);
      codegen_debugloc(c, ast_childlast(body));
      LLVMBuildRet(c->builder, ret);
    }

    codegen_debugloc(c, NULL);
  }

  codegen_finishfun(c);
  return true;
}

static bool genfun_be(compile_t* c, reach_type_t* t, reach_method_t* m)
{
  compile_method_t* c_m = (compile_method_t*)m->c_method;
  pony_assert(c_m->func != NULL);
  pony_assert(c_m->func_handler != NULL);

  AST_GET_CHILDREN(m->r_fun, cap, id, typeparams, params, result, can_error,
    body);

  // Generate the handler.
  codegen_startfun(c, c_m->func_handler, c_m->di_file, c_m->di_method, false);
  name_params(c, t, m, params, c_m->func_handler);

  LLVMValueRef value = gen_expr(c, body);

  if(value == NULL)
    return false;

  codegen_scope_lifetime_end(c);
  if(value != GEN_NOVALUE)
    LLVMBuildRetVoid(c->builder);

  codegen_finishfun(c);

  // Generate the sender.
  codegen_startfun(c, c_m->func, NULL, NULL, false);
  size_t buf_size = (m->param_count + 1) * sizeof(LLVMValueRef);
  LLVMValueRef* param_vals = (LLVMValueRef*)ponyint_pool_alloc_size(buf_size);
  LLVMGetParams(c_m->func, param_vals);

  // Send the arguments in a message to 'this'.
  gen_send_message(c, m, param_vals, params);

  // Return None.
  LLVMBuildRet(c->builder, c->none_instance);
  codegen_finishfun(c);

  ponyint_pool_free_size(buf_size, param_vals);

  // Add the dispatch case.
  LLVMTypeRef msg_type_ptr = LLVMPointerType(c_m->msg_type, 0);
  add_dispatch_case(c, t, params, m->vtable_index, c_m->func_handler,
    c_m->func_type, msg_type_ptr);

  return true;
}

static bool genfun_new(compile_t* c, reach_type_t* t, reach_method_t* m)
{
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  compile_method_t* c_m = (compile_method_t*)m->c_method;
  pony_assert(c_m->func != NULL);

  AST_GET_CHILDREN(m->r_fun, cap, id, typeparams, params, result, can_error,
    body);

  codegen_startfun(c, c_m->func, c_m->di_file, c_m->di_method, false);
  name_params(c, t, m, params, c_m->func);

  LLVMValueRef value = gen_expr(c, body);

  if(value == NULL)
    return false;

  // Return 'this'.
  if(c_t->primitive == NULL)
    value = LLVMGetParam(c_m->func, 0);

  codegen_scope_lifetime_end(c);
  codegen_debugloc(c, ast_childlast(body));
  if(t->underlying == TK_CLASS)
    LLVMBuildRetVoid(c->builder);
  else
    LLVMBuildRet(c->builder, value);
  codegen_debugloc(c, NULL);

  codegen_finishfun(c);
  return true;
}

static bool genfun_newbe(compile_t* c, reach_type_t* t, reach_method_t* m)
{
  compile_method_t* c_m = (compile_method_t*)m->c_method;
  pony_assert(c_m->func != NULL);
  pony_assert(c_m->func_handler != NULL);

  AST_GET_CHILDREN(m->r_fun, cap, id, typeparams, params, result, can_error,
    body);

  // Generate the handler.
  codegen_startfun(c, c_m->func_handler, c_m->di_file, c_m->di_method, false);
  name_params(c, t, m, params, c_m->func_handler);

  LLVMValueRef value = gen_expr(c, body);

  if(value == NULL)
    return false;

  codegen_scope_lifetime_end(c);
  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);

  // Generate the sender.
  codegen_startfun(c, c_m->func, NULL, NULL, false);
  size_t buf_size = (m->param_count + 1) * sizeof(LLVMValueRef);
  LLVMValueRef* param_vals = (LLVMValueRef*)ponyint_pool_alloc_size(buf_size);
  LLVMGetParams(c_m->func, param_vals);

  // Send the arguments in a message to 'this'.
  gen_send_message(c, m, param_vals, params);

  // Return 'this'.
  LLVMBuildRet(c->builder, param_vals[0]);
  codegen_finishfun(c);

  ponyint_pool_free_size(buf_size, param_vals);

  // Add the dispatch case.
  LLVMTypeRef msg_type_ptr = LLVMPointerType(c_m->msg_type, 0);
  add_dispatch_case(c, t, params, m->vtable_index, c_m->func_handler,
    c_m->func_type, msg_type_ptr);

  return true;
}

static void copy_subordinate(reach_method_t* m)
{
  compile_method_t* c_m = (compile_method_t*)m->c_method;
  reach_method_t* m2 = m->subordinate;

  while(m2 != NULL)
  {
    compile_method_t* c_m2 = (compile_method_t*)m2->c_method;
    c_m2->func_type = c_m->func_type;
    c_m2->func = c_m->func;
    m2 = m2->subordinate;
  }
}

static bool genfun_allocator(compile_t* c, reach_type_t* t)
{
  switch(t->underlying)
  {
    case TK_PRIMITIVE:
    case TK_STRUCT:
    case TK_CLASS:
    case TK_ACTOR:
      break;

    default:
      return true;
  }

  compile_type_t* c_t = (compile_type_t*)t->c_type;

  // No allocator for machine word types or pointers.
  if((c_t->primitive != NULL) || is_pointer(t->ast) || is_maybe(t->ast))
    return true;

  const char* funname = genname_alloc(t->name);
  LLVMTypeRef ftype = LLVMFunctionType(c_t->use_type, NULL, 0, false);
  LLVMValueRef fun = codegen_addfun(c, funname, ftype, true);
  if(t->underlying != TK_PRIMITIVE)
  {
    LLVMTypeRef elem = LLVMGetElementType(c_t->use_type);
    size_t size = (size_t)LLVMABISizeOfType(c->target_data, elem);
#if PONY_LLVM >= 309
    LLVM_DECLARE_ATTRIBUTEREF(noalias_attr, noalias, 0);
    LLVM_DECLARE_ATTRIBUTEREF(deref_attr, dereferenceable, size);
    LLVM_DECLARE_ATTRIBUTEREF(align_attr, align, HEAP_MIN);

    LLVMAddAttributeAtIndex(fun, LLVMAttributeReturnIndex, noalias_attr);
    LLVMAddAttributeAtIndex(fun, LLVMAttributeReturnIndex, deref_attr);
    LLVMAddAttributeAtIndex(fun, LLVMAttributeReturnIndex, align_attr);
#else
    LLVMSetReturnNoAlias(fun);
    LLVMSetDereferenceable(fun, 0, size);
#endif
  }
  codegen_startfun(c, fun, NULL, NULL, false);

  LLVMValueRef result;

  switch(t->underlying)
  {
    case TK_PRIMITIVE:
    case TK_STRUCT:
    case TK_CLASS:
      // Allocate the object or return the global instance.
      result = gencall_alloc(c, t);
      break;

    case TK_ACTOR:
      // Allocate the actor.
      result = gencall_create(c, t);
      break;

    default:
      pony_assert(0);
      return false;
  }

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
  return true;
}

static bool genfun_forward(compile_t* c, reach_type_t* t,
  reach_method_name_t* n,  reach_method_t* m)
{
  compile_method_t* c_m = (compile_method_t*)m->c_method;
  pony_assert(c_m->func != NULL);

  reach_method_t* m2 = reach_method(t, m->cap, n->name, m->typeargs);
  pony_assert(m2 != NULL);
  pony_assert(m2 != m);
  compile_method_t* c_m2 = (compile_method_t*)m2->c_method;

  codegen_startfun(c, c_m->func, c_m->di_file, c_m->di_method, m->cap == TK_AT);

  int count = LLVMCountParams(c_m->func);
  size_t buf_size = count * sizeof(LLVMValueRef);

  LLVMValueRef* args = (LLVMValueRef*)ponyint_pool_alloc_size(buf_size);
  args[0] = LLVMGetParam(c_m->func, 0);

  for(int i = 1; i < count; i++)
  {
    LLVMValueRef value = LLVMGetParam(c_m->func, i);
    args[i] = gen_assign_cast(c,
      ((compile_type_t*)m2->params[i - 1].type->c_type)->use_type, value,
      m->params[i - 1].type->ast_cap);
  }

  codegen_debugloc(c, m2->r_fun);
  LLVMValueRef ret = codegen_call(c, c_m2->func, args, count, m->cap != TK_AT);
  codegen_debugloc(c, NULL);
  ret = gen_assign_cast(c, ((compile_type_t*)m->result->c_type)->use_type, ret,
    m2->result->ast_cap);
  LLVMBuildRet(c->builder, ret);
  codegen_finishfun(c);
  return true;
}

void genfun_param_attrs(compile_t* c, reach_type_t* t, reach_method_t* m,
  LLVMValueRef fun)
{
#if PONY_LLVM >= 309
  LLVM_DECLARE_ATTRIBUTEREF(noalias_attr, noalias, 0);
  LLVM_DECLARE_ATTRIBUTEREF(readonly_attr, readonly, 0);
#else
  (void)c;
#endif

  LLVMValueRef param = LLVMGetFirstParam(fun);
  reach_type_t* type = t;
  token_id cap = m->cap;
  int i = 0;
  int offset = 1;

  if(cap == TK_AT)
  {
    i = 1;
    offset = 0;
  }

  while(param != NULL)
  {
    LLVMTypeRef m_type = LLVMTypeOf(param);
    if(LLVMGetTypeKind(m_type) == LLVMPointerTypeKind)
    {
      if(i > 0)
      {
        type = m->params[i-1].type;
        cap = m->params[i-1].cap;
      } else if(ast_id(m->r_fun) == TK_NEW) {
        param = LLVMGetNextParam(param);
        ++i;
        continue;
      }

      if(type->underlying != TK_ACTOR)
      {
        switch(cap)
        {
          case TK_ISO:
#if PONY_LLVM >= 309
            LLVMAddAttributeAtIndex(fun, i + offset, noalias_attr);
#else
            (void)offset;
            LLVMAddAttribute(param, LLVMNoAliasAttribute);
#endif
            break;
          case TK_TRN:
          case TK_REF:
            break;
          case TK_VAL:
          case TK_TAG:
#if PONY_LLVM >= 309
            LLVMAddAttributeAtIndex(fun, i + offset, noalias_attr);
            LLVMAddAttributeAtIndex(fun, i + offset, readonly_attr);
#else
            LLVMAddAttribute(param, LLVMNoAliasAttribute);
            LLVMAddAttribute(param, LLVMReadOnlyAttribute);
#endif
            break;
          case TK_BOX:
#if PONY_LLVM >= 309
            LLVMAddAttributeAtIndex(fun, i + offset, readonly_attr);
#else
            LLVMAddAttribute(param, LLVMReadOnlyAttribute);
#endif
            break;
          default:
            pony_assert(0);
        }
      }
    }
    param = LLVMGetNextParam(param);
    ++i;
  }
}

void genfun_allocate_compile_methods(compile_t* c, reach_type_t* t)
{
  (void)c;

  size_t i = HASHMAP_BEGIN;
  reach_method_name_t* n;

  while((n = reach_method_names_next(&t->methods, &i)) != NULL)
  {
    size_t j = HASHMAP_BEGIN;
    reach_method_t* m;

    while((m = reach_mangled_next(&n->r_mangled, &j)) != NULL)
    {
      compile_method_t* c_m = POOL_ALLOC(compile_method_t);
      memset(c_m, 0, sizeof(compile_method_t));
      c_m->free_fn = compile_method_free;
      m->c_method = (compile_opaque_t*)c_m;
    }
  }
}

bool genfun_method_sigs(compile_t* c, reach_type_t* t)
{
  size_t i = HASHMAP_BEGIN;
  reach_method_name_t* n;

  while((n = reach_method_names_next(&t->methods, &i)) != NULL)
  {
    size_t j = HASHMAP_BEGIN;
    reach_method_t* m;

    while((m = reach_mangled_next(&n->r_mangled, &j)) != NULL)
    {
      make_prototype(c, t, n, m);
      copy_subordinate(m);
    }
  }

  if(!genfun_allocator(c, t))
    return false;

  return true;
}

bool genfun_method_bodies(compile_t* c, reach_type_t* t)
{
  switch(t->underlying)
  {
    case TK_PRIMITIVE:
    case TK_STRUCT:
    case TK_CLASS:
    case TK_ACTOR:
      break;

    default:
      return true;
  }

  size_t i = HASHMAP_BEGIN;
  reach_method_name_t* n;

  while((n = reach_method_names_next(&t->methods, &i)) != NULL)
  {
    size_t j = HASHMAP_BEGIN;
    reach_method_t* m;

    while((m = reach_mangled_next(&n->r_mangled, &j)) != NULL)
    {
      if(m->intrinsic)
        continue;

      if(m->forwarding)
      {
        if(!genfun_forward(c, t, n, m))
          return false;
      } else {
        switch(ast_id(m->r_fun))
        {
          case TK_NEW:
            if(t->underlying == TK_ACTOR)
            {
              if(!genfun_newbe(c, t, m))
                return false;
            } else {
              if(!genfun_new(c, t, m))
                return false;
            }
            break;

          case TK_BE:
            if(!genfun_be(c, t, m))
              return false;
            break;

          case TK_FUN:
            if(!genfun_fun(c, t, m))
              return false;
            break;

          default:
            pony_assert(0);
            return false;
        }
      }
    }
  }

  return true;
}

static bool need_primitive_call(compile_t* c, const char* method)
{
  size_t i = HASHMAP_BEGIN;
  reach_type_t* t;

  while((t = reach_types_next(&c->reach->types, &i)) != NULL)
  {
    if(t->underlying != TK_PRIMITIVE)
      continue;

    reach_method_name_t* n = reach_method_name(t, method);

    if(n == NULL)
      continue;

    return true;
  }

  return false;
}

static void primitive_call(compile_t* c, const char* method)
{
  size_t i = HASHMAP_BEGIN;
  reach_type_t* t;

  while((t = reach_types_next(&c->reach->types, &i)) != NULL)
  {
    if(t->underlying != TK_PRIMITIVE)
      continue;

    reach_method_t* m = reach_method(t, TK_NONE, method, NULL);

    if(m == NULL)
      continue;

    compile_type_t* c_t = (compile_type_t*)t->c_type;
    compile_method_t* c_m = (compile_method_t*)m->c_method;
    LLVMValueRef value = codegen_call(c, c_m->func, &c_t->instance, 1, true);

    if(c->str__final == method)
      LLVMSetInstructionCallConv(value, LLVMCCallConv);
  }
}

void genfun_primitive_calls(compile_t* c)
{
  LLVMTypeRef fn_type = NULL;

  if(need_primitive_call(c, c->str__init))
  {
    fn_type = LLVMFunctionType(c->void_type, NULL, 0, false);
    const char* fn_name = genname_program_fn(c->filename, "primitives_init");
    c->primitives_init = LLVMAddFunction(c->module, fn_name, fn_type);

    codegen_startfun(c, c->primitives_init, NULL, NULL, false);
    primitive_call(c, c->str__init);
    LLVMBuildRetVoid(c->builder);
    codegen_finishfun(c);
  }

  if(need_primitive_call(c, c->str__final))
  {
    if(fn_type == NULL)
      fn_type = LLVMFunctionType(c->void_type, NULL, 0, false);
    const char* fn_name = genname_program_fn(c->filename, "primitives_final");
    c->primitives_final = LLVMAddFunction(c->module, fn_name, fn_type);

    codegen_startfun(c, c->primitives_final, NULL, NULL, false);
    primitive_call(c, c->str__final);
    LLVMBuildRetVoid(c->builder);
    codegen_finishfun(c);
  }
}
