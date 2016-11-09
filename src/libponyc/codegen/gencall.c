#include "gencall.h"
#include "genoperator.h"
#include "genreference.h"
#include "genexpr.h"
#include "gendesc.h"
#include "genfun.h"
#include "genname.h"
#include "genopt.h"
#include "gentrace.h"
#include "../pkg/platformfuns.h"
#include "../type/cap.h"
#include "../type/subtype.h"
#include "../ast/stringtab.h"
#include "../../libponyrt/mem/pool.h"
#include "../../libponyrt/mem/heap.h"
#include <string.h>
#include <assert.h>

static LLVMValueRef invoke_fun(compile_t* c, LLVMValueRef fun,
  LLVMValueRef* args, int count, const char* ret, bool setcc)
{
  if(fun == NULL)
    return NULL;

  LLVMBasicBlockRef this_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef then_block = LLVMInsertBasicBlockInContext(c->context,
    this_block, "invoke");
  LLVMMoveBasicBlockAfter(then_block, this_block);
  LLVMBasicBlockRef else_block = c->frame->invoke_target;

  LLVMValueRef invoke = LLVMBuildInvoke(c->builder, fun, args, count,
    then_block, else_block, ret);

  if(setcc)
    LLVMSetInstructionCallConv(invoke, c->callconv);

  LLVMPositionBuilderAtEnd(c->builder, then_block);
  return invoke;
}

static bool special_case_operator(compile_t* c, ast_t* ast,
  LLVMValueRef *value, bool short_circuit, bool native128)
{
  AST_GET_CHILDREN(ast, positional, named, postfix);
  AST_GET_CHILDREN(postfix, left, method);

  ast_t* right = ast_child(positional);
  const char* name = ast_name(method);
  *value = NULL;

  codegen_debugloc(c, ast);

  if(name == c->str_add)
    *value = gen_add(c, left, right);
  else if(name == c->str_sub)
    *value = gen_sub(c, left, right);
  else if((name == c->str_mul) && native128)
    *value = gen_mul(c, left, right);
  else if((name == c->str_div) && native128)
    *value = gen_div(c, left, right);
  else if((name == c->str_mod) && native128)
    *value = gen_mod(c, left, right);
  else if(name == c->str_neg)
    *value = gen_neg(c, left);
  else if((name == c->str_and) && short_circuit)
    *value = gen_and_sc(c, left, right);
  else if((name == c->str_or) && short_circuit)
    *value = gen_or_sc(c, left, right);
  else if((name == c->str_and) && !short_circuit)
    *value = gen_and(c, left, right);
  else if((name == c->str_or) && !short_circuit)
    *value = gen_or(c, left, right);
  else if(name == c->str_xor)
    *value = gen_xor(c, left, right);
  else if(name == c->str_not)
    *value = gen_not(c, left);
  else if(name == c->str_shl)
    *value = gen_shl(c, left, right);
  else if(name == c->str_shr)
    *value = gen_shr(c, left, right);
  else if(name == c->str_eq)
    *value = gen_eq(c, left, right);
  else if(name == c->str_ne)
    *value = gen_ne(c, left, right);
  else if(name == c->str_lt)
    *value = gen_lt(c, left, right);
  else if(name == c->str_le)
    *value = gen_le(c, left, right);
  else if(name == c->str_ge)
    *value = gen_ge(c, left, right);
  else if(name == c->str_gt)
    *value = gen_gt(c, left, right);

  codegen_debugloc(c, NULL);
  return *value != NULL;
}

static LLVMValueRef special_case_platform(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, positional, named, postfix);
  AST_GET_CHILDREN(postfix, receiver, method);

  const char* method_name = ast_name(method);
  bool is_target;

  if(os_is_target(method_name, c->opt->release, &is_target, c->opt))
    return LLVMConstInt(c->ibool, is_target ? 1 : 0, false);

  ast_error(c->opt->check.errors, ast, "unknown Platform setting");
  return NULL;
}

static bool special_case_call(compile_t* c, ast_t* ast, LLVMValueRef* value)
{
  AST_GET_CHILDREN(ast, positional, named, postfix);

  if((ast_id(postfix) != TK_FUNREF) || (ast_id(named) != TK_NONE))
    return false;

  AST_GET_CHILDREN(postfix, receiver, method);
  ast_t* receiver_type = ast_type(receiver);

  if(ast_id(receiver_type) != TK_NOMINAL)
    return false;

  AST_GET_CHILDREN(receiver_type, package, id);

  if(ast_name(package) != c->str_builtin)
    return false;

  const char* name = ast_name(id);

  if(name == c->str_Bool)
    return special_case_operator(c, ast, value, true, true);

  if((name == c->str_I8) ||
    (name == c->str_I16) ||
    (name == c->str_I32) ||
    (name == c->str_I64) ||
    (name == c->str_ILong) ||
    (name == c->str_ISize) ||
    (name == c->str_U8) ||
    (name == c->str_U16) ||
    (name == c->str_U32) ||
    (name == c->str_U64) ||
    (name == c->str_ULong) ||
    (name == c->str_USize) ||
    (name == c->str_F32) ||
    (name == c->str_F64)
    )
  {
    return special_case_operator(c, ast, value, false, true);
  }

  if((name == c->str_I128) || (name == c->str_U128))
  {
    bool native128 = target_is_native128(c->opt->triple);
    return special_case_operator(c, ast, value, false, native128);
  }

  if(name == c->str_Platform)
  {
    *value = special_case_platform(c, ast);
    return true;
  }

  return false;
}

static LLVMValueRef dispatch_function(compile_t* c, reach_type_t* t,
  reach_method_t* m, LLVMValueRef l_value)
{
  switch(t->underlying)
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_INTERFACE:
    case TK_TRAIT:
    {
      // Get the function from the vtable.
      LLVMValueRef func = gendesc_vtable(c, l_value, m->vtable_index);

      return LLVMBuildBitCast(c->builder, func,
        LLVMPointerType(m->func_type, 0), "");
    }

    case TK_PRIMITIVE:
    case TK_STRUCT:
    case TK_CLASS:
    case TK_ACTOR:
    {
      // Static, get the actual function.
      return m->func;
    }

    default: {}
  }

  assert(0);
  return NULL;
}

static bool call_needs_receiver(ast_t* postfix, reach_type_t* t)
{
  switch(ast_id(postfix))
  {
    case TK_NEWREF:
    case TK_NEWBEREF:
      break;

    default:
      return true;
  }

  // No receiver if a new primitive.
  if(t->primitive != NULL)
    return false;

  // No receiver if a new Pointer or Maybe.
  if(is_pointer(t->ast) || is_maybe(t->ast))
    return false;

  return true;
}

static void set_descriptor(compile_t* c, reach_type_t* t, LLVMValueRef value)
{
  if(t->underlying == TK_STRUCT)
    return;

  LLVMValueRef desc_ptr = LLVMBuildStructGEP(c->builder, value, 0, "");
  LLVMValueRef store = LLVMBuildStore(c->builder, t->desc, desc_ptr);
  const char id[] = "tbaa";
  LLVMSetMetadata(store, LLVMGetMDKindID(id, sizeof(id) - 1), c->tbaa_descptr);
}

static void set_method_external_nominal(reach_type_t* t, const char* name)
{
  reach_method_name_t* n = reach_method_name(t, name);
  if(n != NULL)
  {
    size_t i = HASHMAP_BEGIN;
    reach_method_t* m;
    while((m = reach_methods_next(&n->r_methods, &i)) != NULL)
    {
      LLVMSetFunctionCallConv(m->func, LLVMCCallConv);
      LLVMSetLinkage(m->func, LLVMExternalLinkage);
    }
  }
}

static void set_method_external_interface(reach_type_t* t, const char* name)
{
  set_method_external_nominal(t, name);

  size_t i = HASHMAP_BEGIN;
  reach_type_t* sub;
  while((sub = reach_type_cache_next(&t->subtypes, &i)) != NULL)
  {
    reach_method_name_t* n = reach_method_name(sub, name);
    if(n == NULL)
      continue;
    size_t j = HASHMAP_BEGIN;
    reach_method_t* m;
    while((m = reach_methods_next(&n->r_methods, &j)) != NULL)
    {
      LLVMSetFunctionCallConv(m->func, LLVMCCallConv);
      LLVMSetLinkage(m->func, LLVMExternalLinkage);
    }
  }
}

LLVMValueRef gen_funptr(compile_t* c, ast_t* ast)
{
  assert((ast_id(ast) == TK_FUNREF) || (ast_id(ast) == TK_BEREF));
  AST_GET_CHILDREN(ast, receiver, method);
  ast_t* typeargs = NULL;

  // Dig through function qualification.
  switch(ast_id(receiver))
  {
    case TK_BEREF:
    case TK_FUNREF:
      typeargs = method;
      AST_GET_CHILDREN_NO_DECL(receiver, receiver, method);
      break;

    default: {}
  }

  // Generate the receiver.
  LLVMValueRef value = gen_expr(c, receiver);

  // Get the receiver type.
  ast_t* type = ast_type(receiver);
  reach_type_t* t = reach_type(c->reach, type);
  assert(t != NULL);

  const char* name = ast_name(method);
  token_id cap = cap_dispatch(type);
  reach_method_t* m = reach_method(t, cap, name, typeargs);
  LLVMValueRef funptr = dispatch_function(c, t, m, value);

  if(c->linkage != LLVMExternalLinkage)
  {
    // We must reset the function linkage and calling convention since we're
    // passing a function pointer to a FFI call.
    switch(t->underlying)
    {
      case TK_PRIMITIVE:
      case TK_STRUCT:
      case TK_CLASS:
      case TK_ACTOR:
        set_method_external_nominal(t, name);
        break;
      case TK_UNIONTYPE:
      case TK_ISECTTYPE:
      case TK_INTERFACE:
      case TK_TRAIT:
        set_method_external_interface(t, name);
        break;
      default:
        assert(0);
        break;
    }
  }

  return funptr;
}

void gen_send_message(compile_t* c, reach_method_t* m, LLVMValueRef args[],
  ast_t* args_ast)
{
  // Allocate the message, setting its size and ID.
  size_t msg_size = (size_t)LLVMABISizeOfType(c->target_data, m->msg_type);
  LLVMTypeRef msg_type_ptr = LLVMPointerType(m->msg_type, 0);

  LLVMValueRef msg_args[3];

  msg_args[0] = LLVMConstInt(c->i32, ponyint_pool_index(msg_size), false);
  msg_args[1] = LLVMConstInt(c->i32, m->vtable_index, false);
  LLVMValueRef msg = gencall_runtime(c, "pony_alloc_msg", msg_args, 2, "");
  LLVMValueRef msg_ptr = LLVMBuildBitCast(c->builder, msg, msg_type_ptr, "");

  for(unsigned int i = 0; i < m->param_count; i++)
  {
    LLVMValueRef arg_ptr = LLVMBuildStructGEP(c->builder, msg_ptr, i + 3, "");
    LLVMBuildStore(c->builder, args[i+1], arg_ptr);
  }

  // Trace while populating the message contents.
  ast_t* params = ast_childidx(m->r_fun, 3);
  ast_t* param = ast_child(params);
  ast_t* arg_ast = ast_child(args_ast);
  bool need_trace = false;

  while(param != NULL)
  {
    if(gentrace_needed(c, ast_type(arg_ast), ast_type(param)))
    {
      need_trace = true;
      break;
    }

    param = ast_sibling(param);
    arg_ast = ast_sibling(arg_ast);
  }

  LLVMValueRef ctx = codegen_ctx(c);

  if(need_trace)
  {
    gencall_runtime(c, "pony_gc_send", &ctx, 1, "");
    param = ast_child(params);
    arg_ast = ast_child(args_ast);

    for(size_t i = 0; i < m->param_count; i++)
    {
      gentrace(c, ctx, args[i+1], ast_type(arg_ast), ast_type(param));
      param = ast_sibling(param);
      arg_ast = ast_sibling(arg_ast);
    }

    gencall_runtime(c, "pony_send_done", &ctx, 1, "");
  }

  // Send the message.
  msg_args[0] = ctx;
  msg_args[1] = LLVMBuildBitCast(c->builder, args[0], c->object_ptr, "");
  msg_args[2] = msg;
  gencall_runtime(c, "pony_sendv", msg_args, 3, "");
}

typedef struct call_tuple_indices_t
{
  size_t* data;
  size_t count;
  size_t alloc;
} call_tuple_indices_t;

static void tuple_indices_push(call_tuple_indices_t* ti, size_t idx)
{
  if(ti->count == ti->alloc)
  {
    size_t* tmp_data =
      (size_t*)ponyint_pool_alloc_size(2 * ti->alloc * sizeof(size_t));
    memcpy(tmp_data, ti->data, ti->count * sizeof(size_t));
    ponyint_pool_free_size(ti->alloc * sizeof(size_t), ti->data);
    ti->alloc *= 2;
    ti->data = tmp_data;
  }
  ti->data[ti->count++] = idx;
}

static size_t tuple_indices_pop(call_tuple_indices_t* ti)
{
  assert(ti->count > 0);

  return ti->data[--ti->count];
}

static bool behaviour_in_every_subtype(reach_type_t* t, const char* method_name)
{
  switch(t->underlying)
  {
    case TK_CLASS:
    case TK_STRUCT:
    case TK_PRIMITIVE:
      return false;

    case TK_ACTOR:
      return true;

    default: {}
  }

  size_t i = HASHMAP_BEGIN;
  reach_type_t* sub = t;
  do
  {
    reach_method_name_t* n = reach_method_name(sub, method_name);

    if(n == NULL)
      continue;

    size_t j = HASHMAP_BEGIN;
    // The kind of a method cannot vary within a type so we only need to check
    // one reach method.
    reach_method_t* m = reach_methods_next(&n->r_methods, &j);

    if(m == NULL)
      continue;

    if(ast_id(m->r_fun) == TK_FUN)
      return false;

    if(ast_id(m->r_fun) == TK_NEW)
    {
      switch(sub->underlying)
      {
        case TK_CLASS:
        case TK_PRIMITIVE:
          return false;

        default: {}
      }
    }
  } while((sub = reach_type_cache_next(&t->subtypes, &i)) != NULL);

  return true;
}

LLVMValueRef gen_call(compile_t* c, ast_t* ast)
{
  // Special case calls.
  LLVMValueRef special;

  if(special_case_call(c, ast, &special))
    return special;

  AST_GET_CHILDREN(ast, positional, named, postfix);
  AST_GET_CHILDREN(postfix, receiver, method);
  ast_t* typeargs = NULL;

  // Dig through function qualification.
  switch(ast_id(receiver))
  {
    case TK_NEWREF:
    case TK_NEWBEREF:
    case TK_BEREF:
    case TK_FUNREF:
    case TK_BECHAIN:
    case TK_FUNCHAIN:
      typeargs = method;
      AST_GET_CHILDREN_NO_DECL(receiver, receiver, method);
      break;

    default: {}
  }

  // Get the receiver type.
  const char* method_name = ast_name(method);
  ast_t* type = ast_type(receiver);
  reach_type_t* t = reach_type(c->reach, type);
  assert(t != NULL);

  // Generate the arguments.
  size_t count = ast_childcount(positional) + 1;
  size_t buf_size = count * sizeof(void*);

  LLVMValueRef* args = (LLVMValueRef*)ponyint_pool_alloc_size(buf_size);
  ast_t* arg = ast_child(positional);
  int i = 1;

  while(arg != NULL)
  {
    LLVMValueRef value = gen_expr(c, arg);

    if(value == NULL)
    {
      ponyint_pool_free_size(buf_size, args);
      return NULL;
    }

    args[i] = value;
    arg = ast_sibling(arg);
    i++;
  }

  bool is_new_call = false;

  // Generate the receiver. Must be done after the arguments because the args
  // could change things in the receiver expression that must be accounted for.
  if(call_needs_receiver(postfix, t))
  {
    switch(ast_id(postfix))
    {
      case TK_NEWREF:
      case TK_NEWBEREF:
      {
        call_tuple_indices_t tuple_indices = {NULL, 0, 4};
        tuple_indices.data =
          (size_t*)ponyint_pool_alloc_size(4 * sizeof(size_t));

        ast_t* current = ast;
        ast_t* parent = ast_parent(current);
        while((parent != NULL) && (ast_id(parent) != TK_ASSIGN) &&
          (ast_id(parent) != TK_CALL))
        {
          if(ast_id(parent) == TK_TUPLE)
          {
            size_t index = 0;
            ast_t* child = ast_child(parent);
            while(current != child)
            {
              ++index;
              child = ast_sibling(child);
            }
            tuple_indices_push(&tuple_indices, index);
          }
          current = parent;
          parent = ast_parent(current);
        }

        // If we're constructing an embed field, pass a pointer to the field
        // as the receiver. Otherwise, allocate an object.
        if((parent != NULL) && (ast_id(parent) == TK_ASSIGN))
        {
          size_t index = 1;
          current = ast_childidx(parent, 1);
          while((ast_id(current) == TK_TUPLE) || (ast_id(current) == TK_SEQ))
          {
            parent = current;
            if(ast_id(current) == TK_TUPLE)
            {
              // If there are no indices left, we're destructuring a tuple.
              // Errors in those cases have already been catched by the expr
              // pass.
              if(tuple_indices.count == 0)
                break;
              index = tuple_indices_pop(&tuple_indices);
              current = ast_childidx(parent, index);
            } else {
              current = ast_childlast(parent);
            }
          }
          if(ast_id(current) == TK_EMBEDREF)
          {
            args[0] = gen_fieldptr(c, current);
            set_descriptor(c, t, args[0]);
          } else {
            args[0] = gencall_alloc(c, t);
          }
        } else {
          args[0] = gencall_alloc(c, t);
        }
        is_new_call = true;
        ponyint_pool_free_size(tuple_indices.alloc * sizeof(size_t),
          tuple_indices.data);
        break;
      }

      case TK_BEREF:
      case TK_FUNREF:
      case TK_BECHAIN:
      case TK_FUNCHAIN:
        args[0] = gen_expr(c, receiver);
        break;

      default:
        assert(0);
        return NULL;
    }
  } else {
    // Use a null for the receiver type.
    args[0] = LLVMConstNull(t->use_type);
  }

  // Static or virtual dispatch.
  token_id cap = cap_dispatch(type);
  reach_method_t* m = reach_method(t, cap, method_name, typeargs);
  LLVMValueRef func = dispatch_function(c, t, m, args[0]);

  bool is_message = false;

  if((ast_id(postfix) == TK_NEWBEREF) || (ast_id(postfix) == TK_BEREF))
  {
    switch(t->underlying)
    {
      case TK_ACTOR:
        is_message = true;
        break;

      case TK_UNIONTYPE:
      case TK_ISECTTYPE:
      case TK_INTERFACE:
      case TK_TRAIT:
        is_message = behaviour_in_every_subtype(t, method_name);
        break;

      default: {}
    }
  }

  // Cast the arguments to the parameter types.
  LLVMTypeRef f_type = LLVMGetElementType(LLVMTypeOf(func));
  LLVMTypeRef* params = (LLVMTypeRef*)ponyint_pool_alloc_size(buf_size);
  LLVMGetParamTypes(f_type, params);

  arg = ast_child(positional);
  i = 1;

  LLVMValueRef r = NULL;

  if(is_message)
  {
    // If we're sending a message, trace and send here instead of calling the
    // sender to trace the most specific types possible.
    while(arg != NULL)
    {
      args[i] = gen_assign_cast(c, params[i], args[i], ast_type(arg));
      arg = ast_sibling(arg);
      i++;
    }

    token_id cap = cap_dispatch(type);
    reach_method_t* m = reach_method(t, cap, method_name, typeargs);

    codegen_debugloc(c, ast);
    gen_send_message(c, m, args, positional);
    codegen_debugloc(c, NULL);
    r = args[0];
  } else {
    while(arg != NULL)
    {
      args[i] = gen_assign_cast(c, params[i], args[i], ast_type(arg));
      arg = ast_sibling(arg);
      i++;
    }

    if(func != NULL)
    {
      // If we can error out and we have an invoke target, generate an invoke
      // instead of a call.
      codegen_debugloc(c, ast);

      if(ast_canerror(ast) && (c->frame->invoke_target != NULL))
        r = invoke_fun(c, func, args, i, "", true);
      else
        r = codegen_call(c, func, args, i);

      if(is_new_call)
      {
        LLVMValueRef md = LLVMMDNodeInContext(c->context, NULL, 0);
        LLVMSetMetadataStr(r, "pony.newcall", md);
      }

      codegen_debugloc(c, NULL);
    }
  }

  // Class constructors return void, expression result is the receiver.
  if(((ast_id(postfix) == TK_NEWREF) || (ast_id(postfix) == TK_NEWBEREF)) &&
     (t->underlying == TK_CLASS))
    r = args[0];

  // Chained methods forward their receiver.
  if((ast_id(postfix) == TK_BECHAIN) || (ast_id(postfix) == TK_FUNCHAIN))
    r = args[0];

  ponyint_pool_free_size(buf_size, args);
  ponyint_pool_free_size(buf_size, params);
  return r;
}

LLVMValueRef gen_pattern_eq(compile_t* c, ast_t* pattern, LLVMValueRef r_value)
{
  // This is used for structural equality in pattern matching.
  ast_t* pattern_type = ast_type(pattern);

  if(ast_id(pattern_type) == TK_NOMINAL)
  {
    AST_GET_CHILDREN(pattern_type, package, id);

    // Special case equality on primitive types.
    if(ast_name(package) == c->str_builtin)
    {
      const char* name = ast_name(id);

      if((name == c->str_Bool) ||
        (name == c->str_I8) ||
        (name == c->str_I16) ||
        (name == c->str_I32) ||
        (name == c->str_I64) ||
        (name == c->str_I128) ||
        (name == c->str_ILong) ||
        (name == c->str_ISize) ||
        (name == c->str_U8) ||
        (name == c->str_U16) ||
        (name == c->str_U32) ||
        (name == c->str_U64) ||
        (name == c->str_U128) ||
        (name == c->str_ULong) ||
        (name == c->str_USize) ||
        (name == c->str_F32) ||
        (name == c->str_F64)
        )
      {
        return gen_eq_rvalue(c, pattern, r_value);
      }
    }
  }

  // Generate the receiver.
  LLVMValueRef l_value = gen_expr(c, pattern);
  reach_type_t* t = reach_type(c->reach, pattern_type);
  assert(t != NULL);

  // Static or virtual dispatch.
  token_id cap = cap_dispatch(pattern_type);
  reach_method_t* m = reach_method(t, cap, c->str_eq, NULL);
  LLVMValueRef func = dispatch_function(c, t, m, l_value);

  if(func == NULL)
    return NULL;

  // Call the function. We know it isn't partial.
  LLVMValueRef args[2];
  args[0] = l_value;
  args[1] = r_value;

  codegen_debugloc(c, pattern);
  LLVMValueRef result = codegen_call(c, func, args, 2);
  codegen_debugloc(c, NULL);

  return result;
}

static LLVMValueRef declare_ffi_vararg(compile_t* c, const char* f_name,
  reach_type_t* t, bool err)
{
  LLVMTypeRef f_type = LLVMFunctionType(t->use_type, NULL, 0, true);
  LLVMValueRef func = LLVMAddFunction(c->module, f_name, f_type);

  if(!err)
  {
#if PONY_LLVM >= 309
    LLVM_DECLARE_ATTRIBUTEREF(nounwind_attr, nounwind, 0);

    LLVMAddAttributeAtIndex(func, LLVMAttributeFunctionIndex, nounwind_attr);
#else
    LLVMAddFunctionAttr(func, LLVMNoUnwindAttribute);
#endif
  }

  return func;
}

static LLVMValueRef declare_ffi(compile_t* c, const char* f_name,
  reach_type_t* t, ast_t* args, bool err, bool intrinsic)
{
  ast_t* last_arg = ast_childlast(args);

  if((last_arg != NULL) && (ast_id(last_arg) == TK_ELLIPSIS))
    return declare_ffi_vararg(c, f_name, t, err);

  int count = (int)ast_childcount(args);
  size_t buf_size = count * sizeof(LLVMTypeRef);
  LLVMTypeRef* f_params = (LLVMTypeRef*)ponyint_pool_alloc_size(buf_size);
  count = 0;

  ast_t* arg = ast_child(args);

  while(arg != NULL)
  {
    ast_t* p_type = ast_type(arg);

    if(p_type == NULL)
      p_type = ast_childidx(arg, 1);

    reach_type_t* pt = reach_type(c->reach, p_type);
    assert(pt != NULL);

    // An intrinsic that takes a Bool should be i1, not ibool.
    if(intrinsic && is_bool(pt->ast))
      f_params[count++] = c->i1;
    else
      f_params[count++] = pt->use_type;

    arg = ast_sibling(arg);
  }

  LLVMTypeRef r_type;

  if(t->underlying == TK_TUPLETYPE)
  {
    // Can't use the named type. Build an unnamed type with the same
    // elements.
    unsigned int count = LLVMCountStructElementTypes(t->use_type);
    size_t buf_size = count * sizeof(LLVMTypeRef);
    LLVMTypeRef* e_types = (LLVMTypeRef*)ponyint_pool_alloc_size(buf_size);
    LLVMGetStructElementTypes(t->use_type, e_types);

    if(intrinsic)
    {
      ast_t* child = ast_child(t->ast);
      size_t i = 0;

      while(child != NULL)
      {
        // A Bool in an intrinsic tuple return type is an i1, not an ibool.
        if(is_bool(child))
          e_types[i] = c->i1;

        child = ast_sibling(child);
        i++;
      }
    }

    r_type = LLVMStructTypeInContext(c->context, e_types, count, false);
    ponyint_pool_free_size(buf_size, e_types);
  } else {
    // An intrinsic that returns a Bool returns an i1, not an ibool.
    if(intrinsic && is_bool(t->ast))
      r_type = c->i1;
    else
      r_type = t->use_type;
  }

  LLVMTypeRef f_type = LLVMFunctionType(r_type, f_params, count, false);
  LLVMValueRef func = LLVMAddFunction(c->module, f_name, f_type);

  if(!err)
  {
#if PONY_LLVM >= 309
    LLVM_DECLARE_ATTRIBUTEREF(nounwind_attr, nounwind, 0);

    LLVMAddAttributeAtIndex(func, LLVMAttributeFunctionIndex, nounwind_attr);
#else
    LLVMAddFunctionAttr(func, LLVMNoUnwindAttribute);
#endif
  }

  ponyint_pool_free_size(buf_size, f_params);
  return func;
}

static LLVMValueRef cast_ffi_arg(compile_t* c, LLVMValueRef arg,
  LLVMTypeRef param)
{
  if(arg == NULL)
    return NULL;

  LLVMTypeRef arg_type = LLVMTypeOf(arg);

  if(param == c->i1)
  {
    // If the parameter is an i1, it must be from an LLVM intrinsic. In that
    // case, the argument must be a Bool encoded as an ibool.
    if(arg_type != c->ibool)
      return NULL;

    // Truncate the Bool's i8 representation to i1.
    return LLVMBuildTrunc(c->builder, arg, c->i1, "");
  }

  switch(LLVMGetTypeKind(param))
  {
    case LLVMPointerTypeKind:
    {
      if(LLVMGetTypeKind(arg_type) == LLVMIntegerTypeKind)
        arg = LLVMBuildIntToPtr(c->builder, arg, param, "");
      else
        arg = LLVMBuildBitCast(c->builder, arg, param, "");

      break;
    }

    default: {}
  }

  return arg;
}

LLVMValueRef gen_ffi(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, id, typeargs, args, named_args, can_err);
  bool err = (ast_id(can_err) == TK_QUESTION);

  // Get the function name, +1 to skip leading @
  const char* f_name = ast_name(id) + 1;

  // Get the return type.
  ast_t* type = ast_type(ast);
  reach_type_t* t = reach_type(c->reach, type);
  assert(t != NULL);

  // Get the function.
  LLVMValueRef func = LLVMGetNamedFunction(c->module, f_name);

  if(func == NULL)
  {
    // If we have no prototype, declare one.
    ast_t* decl = (ast_t*)ast_data(ast);

    if(decl != NULL)
    {
      // Define using the declared types.
      AST_GET_CHILDREN(decl, decl_id, decl_ret, decl_params, decl_err);
      err = (ast_id(decl_err) == TK_QUESTION);
      func = declare_ffi(c, f_name, t, decl_params, err, false);
    } else if(!strncmp(f_name, "llvm.", 5)) {
      // Intrinsic, so use the exact types we supply.
      func = declare_ffi(c, f_name, t, args, err, true);
    } else {
      // Make it varargs.
      func = declare_ffi_vararg(c, f_name, t, err);
    }
  }

  // Generate the arguments.
  int count = (int)ast_childcount(args);
  size_t buf_size = count * sizeof(LLVMValueRef);
  LLVMValueRef* f_args = (LLVMValueRef*)ponyint_pool_alloc_size(buf_size);

  LLVMTypeRef f_type = LLVMGetElementType(LLVMTypeOf(func));
  LLVMTypeRef* f_params = NULL;
  bool vararg = (LLVMIsFunctionVarArg(f_type) != 0);

  if(!vararg)
  {
    f_params = (LLVMTypeRef*)ponyint_pool_alloc_size(buf_size);
    LLVMGetParamTypes(f_type, f_params);
  }

  ast_t* arg = ast_child(args);

  for(int i = 0; i < count; i++)
  {
    f_args[i] = gen_expr(c, arg);

    if(!vararg)
      f_args[i] = cast_ffi_arg(c, f_args[i], f_params[i]);

    if(f_args[i] == NULL)
    {
      ponyint_pool_free_size(buf_size, f_args);
      return NULL;
    }

    arg = ast_sibling(arg);
  }

  // If we can error out and we have an invoke target, generate an invoke
  // instead of a call.
  LLVMValueRef result;
  codegen_debugloc(c, ast);

  if(err && (c->frame->invoke_target != NULL))
    result = invoke_fun(c, func, f_args, count, "", false);
  else
    result = LLVMBuildCall(c->builder, func, f_args, count, "");

  codegen_debugloc(c, NULL);
  ponyint_pool_free_size(buf_size, f_args);

  if(!vararg)
    ponyint_pool_free_size(buf_size, f_params);

  // Special case a None return value, which is used for void functions.
  if(is_none(type))
    return t->instance;

  return result;
}

LLVMValueRef gencall_runtime(compile_t* c, const char *name,
  LLVMValueRef* args, int count, const char* ret)
{
  LLVMValueRef func = LLVMGetNamedFunction(c->module, name);

  if(func == NULL)
    return NULL;

  return LLVMBuildCall(c->builder, func, args, count, ret);
}

LLVMValueRef gencall_create(compile_t* c, reach_type_t* t)
{
  LLVMValueRef args[2];
  args[0] = codegen_ctx(c);
  args[1] = LLVMConstBitCast(t->desc, c->descriptor_ptr);

  LLVMValueRef result = gencall_runtime(c, "pony_create", args, 2, "");
  return LLVMBuildBitCast(c->builder, result, t->use_type, "");
}

LLVMValueRef gencall_alloc(compile_t* c, reach_type_t* t)
{
  // Do nothing for primitives.
  if(t->primitive != NULL)
    return NULL;

  // Do nothing for Pointer and Maybe.
  if(is_pointer(t->ast) || is_maybe(t->ast))
    return NULL;

  // Use the global instance if we have one.
  if(t->instance != NULL)
    return t->instance;

  if(t->underlying == TK_ACTOR)
    return gencall_create(c, t);

  return gencall_allocstruct(c, t);
}

LLVMValueRef gencall_allocstruct(compile_t* c, reach_type_t* t)
{
  // We explicitly want a boxed version.
  // Allocate the object.
  LLVMValueRef args[3];
  args[0] = codegen_ctx(c);

  LLVMValueRef result;

  size_t size = t->abi_size;
  if(size == 0)
    size = 1;

  if(t->final_fn == NULL)
  {
    if(size <= HEAP_MAX)
    {
      uint32_t index = ponyint_heap_index(size);
      args[1] = LLVMConstInt(c->i32, index, false);
      result = gencall_runtime(c, "pony_alloc_small", args, 2, "");
    } else {
      args[1] = LLVMConstInt(c->intptr, size, false);
      result = gencall_runtime(c, "pony_alloc_large", args, 2, "");
    }
  } else {
    args[1] = LLVMConstInt(c->intptr, size, false);
    args[2] = LLVMConstBitCast(t->final_fn, c->final_fn);
    result = gencall_runtime(c, "pony_alloc_final", args, 3, "");
  }

  result = LLVMBuildBitCast(c->builder, result, t->structure_ptr, "");
  set_descriptor(c, t, result);

  return result;
}

void gencall_throw(compile_t* c)
{
  LLVMValueRef func = LLVMGetNamedFunction(c->module, "pony_throw");

  if(c->frame->invoke_target != NULL)
    invoke_fun(c, func, NULL, 0, "", false);
  else
    LLVMBuildCall(c->builder, func, NULL, 0, "");

  LLVMBuildUnreachable(c->builder);
}

void gencall_memcpy(compile_t* c, LLVMValueRef dst, LLVMValueRef src,
  LLVMValueRef n)
{
  LLVMValueRef func = LLVMMemcpy(c->module, target_is_ilp32(c->opt->triple));

  LLVMValueRef args[5];
  args[0] = dst;
  args[1] = src;
  args[2] = n;
  args[3] = LLVMConstInt(c->i32, 1, false);
  args[4] = LLVMConstInt(c->i1, 0, false);
  LLVMBuildCall(c->builder, func, args, 5, "");
}

void gencall_memmove(compile_t* c, LLVMValueRef dst, LLVMValueRef src,
  LLVMValueRef n)
{
  LLVMValueRef func = LLVMMemmove(c->module, target_is_ilp32(c->opt->triple));

  LLVMValueRef args[5];
  args[0] = dst;
  args[1] = src;
  args[2] = n;
  args[3] = LLVMConstInt(c->i32, 1, false);
  args[4] = LLVMConstInt(c->i1, 0, false);
  LLVMBuildCall(c->builder, func, args, 5, "");
}

void gencall_lifetime_start(compile_t* c, LLVMValueRef ptr)
{
  LLVMValueRef func = LLVMLifetimeStart(c->module);
  LLVMTypeRef type = LLVMGetElementType(LLVMTypeOf(ptr));
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, type);

  LLVMValueRef args[2];
  args[0] = LLVMConstInt(c->i64, size, false);
  args[1] = LLVMBuildBitCast(c->builder, ptr, c->void_ptr, "");
  LLVMBuildCall(c->builder, func, args, 2, "");
}

void gencall_lifetime_end(compile_t* c, LLVMValueRef ptr)
{
  LLVMValueRef func = LLVMLifetimeEnd(c->module);
  LLVMTypeRef type = LLVMGetElementType(LLVMTypeOf(ptr));
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, type);

  LLVMValueRef args[2];
  args[0] = LLVMConstInt(c->i64, size, false);
  args[1] = LLVMBuildBitCast(c->builder, ptr, c->void_ptr, "");
  LLVMBuildCall(c->builder, func, args, 2, "");
}
