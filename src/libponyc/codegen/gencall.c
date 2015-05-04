#include "gencall.h"
#include "genoperator.h"
#include "genreference.h"
#include "genexpr.h"
#include "gendesc.h"
#include "genfun.h"
#include "genname.h"
#include "../pkg/platformfuns.h"
#include "../type/subtype.h"
#include "../ast/stringtab.h"
#include <string.h>
#include <assert.h>

static LLVMValueRef invoke_fun(compile_t* c, LLVMValueRef fun,
  LLVMValueRef* args, int count, const char* ret, bool fastcc)
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

  if(fastcc && !c->opt->library)
    LLVMSetInstructionCallConv(invoke, GEN_CALLCONV);

  LLVMPositionBuilderAtEnd(c->builder, then_block);
  return invoke;
}

static LLVMValueRef make_arg(compile_t* c, LLVMTypeRef type, ast_t* arg)
{
  LLVMValueRef value = gen_expr(c, arg);

  if(value == NULL)
    return NULL;

  return gen_assign_cast(c, type, value, ast_type(arg));
}

static bool special_case_operator(compile_t* c, ast_t* ast, LLVMValueRef *value,
  bool short_circuit, bool has_divmod)
{
  AST_GET_CHILDREN(ast, positional, named, postfix);
  AST_GET_CHILDREN(postfix, left, method);

  ast_t* right = ast_child(positional);
  const char* name = ast_name(method);
  *value = NULL;

  if(name == c->str_add)
    *value = gen_add(c, left, right);
  else if(name == c->str_sub)
    *value = gen_sub(c, left, right);
  else if(name == c->str_mul)
    *value = gen_mul(c, left, right);
  else if((name == c->str_div) && has_divmod)
    *value = gen_div(c, left, right);
  else if((name == c->str_mod) && has_divmod)
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
  else
    return false;

  return true;
}

static LLVMValueRef special_case_platform(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, positional, named, postfix);
  AST_GET_CHILDREN(postfix, receiver, method);

  const char* method_name = ast_name(method);
  bool is_target;

  if(os_is_target(method_name, c->opt->release, &is_target))
    return LLVMConstInt(c->i1, is_target ? 1 : 0, false);

  ast_error(ast, "unknown Platform setting");
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

  if(ast_name(package) != c->str_1)
    return false;

  const char* name = ast_name(id);

  if(name == c->str_Bool)
    return special_case_operator(c, ast, value, true, true);

  if((name == c->str_I8) ||
    (name == c->str_I16) ||
    (name == c->str_I32) ||
    (name == c->str_I64) ||
    (name == c->str_U8) ||
    (name == c->str_U16) ||
    (name == c->str_U32) ||
    (name == c->str_U64) ||
    (name == c->str_F32) ||
    (name == c->str_F64)
    )
  {
    return special_case_operator(c, ast, value, false, true);
  }

  if((name == c->str_I128) || (name == c->str_U128))
  {
    bool has_i128;
    os_is_target(OS_HAS_I128_NAME, c->opt->release, &has_i128);
    return special_case_operator(c, ast, value, false, has_i128);
  }

  if(name == c->str_Platform)
  {
    *value = special_case_platform(c, ast);
    return true;
  }

  return false;
}

static LLVMValueRef dispatch_function(compile_t* c, ast_t* from, gentype_t* g,
  LLVMValueRef l_value, const char* method_name, ast_t* typeargs)
{
  LLVMValueRef func;

  if(g->use_type == c->object_ptr)
  {
    // Virtual, get the function by selector colour.
    uint32_t index = genfun_vtable_index(c, g, method_name, typeargs);
    assert(index != (uint32_t)-1);

    // Get the function from the vtable.
    func = gendesc_vtable(c, l_value, index);

    // Cast to the right function type.
    LLVMTypeRef f_type = genfun_sig(c, g, method_name, typeargs);

    if(f_type == NULL)
    {
      ast_error(from, "couldn't create a signature for '%s'", method_name);
      return NULL;
    }

    f_type = LLVMPointerType(f_type, 0);
    func = LLVMBuildBitCast(c->builder, func, f_type, "method");
  } else {
    // Static, get the actual function.
    func = genfun_proto(c, g, method_name, typeargs);

    if(func == NULL)
    {
      ast_error(from, "couldn't locate '%s'", method_name);
      return NULL;
    }
  }

  return func;
}

static bool call_needs_receiver(ast_t* postfix, gentype_t* g)
{
  if(ast_id(postfix) != TK_NEWREF)
    return true;

  // No receiver if a new primitive.
  if(g->primitive != NULL)
    return false;

  // No receiver if a new Pointer.
  if(is_pointer(g->ast))
    return false;

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
      typeargs = method;
      AST_GET_CHILDREN_NO_DECL(receiver, receiver, method);
      break;

    default: {}
  }

  // Generate the receiver type.
  const char* method_name = ast_name(method);
  ast_t* type = ast_type(receiver);
  gentype_t g;

  if(!gentype(c, type, &g))
    return NULL;

  bool need_receiver = call_needs_receiver(postfix, &g);

  // Generate the arguments.
  LLVMTypeRef f_type = genfun_sig(c, &g, method_name, typeargs);

  if(f_type == NULL)
  {
    ast_error(ast, "couldn't create a signature for '%s'", method_name);
    return NULL;
  }

  size_t count = ast_childcount(positional) + need_receiver;

  VLA(LLVMValueRef, args, count);
  VLA(LLVMTypeRef, params, count);
  LLVMGetParamTypes(f_type, params);

  ast_t* arg = ast_child(positional);
  int i = need_receiver;

  while(arg != NULL)
  {
    LLVMValueRef value = make_arg(c, params[i], arg);

    if(value == NULL)
      return NULL;

    args[i] = value;
    arg = ast_sibling(arg);
    i++;
  }

  // Generate the receiver. Must be done after the arguments because the args
  // could change things in the receiver expression that must be accounted for.
  if(need_receiver)
  {
    switch(ast_id(postfix))
    {
      case TK_NEWREF:
      case TK_NEWBEREF:
        args[0] = gencall_alloc(c, &g);
        break;

      case TK_BEREF:
      case TK_FUNREF:
        args[0] = gen_expr(c, receiver);
        break;

      default:
        assert(0);
        return NULL;
    }
  }

  // Always emit location info for a call, to prevent inlining errors. This may
  // be disabled in dispatch_function, if the target function has no debug
  // info set.
  ast_setdebug(ast, true);
  dwarf_location(&c->dwarf, ast);

  // Static or virtual dispatch.
  LLVMValueRef func = dispatch_function(c, ast, &g, args[0], method_name,
    typeargs);

  if(func == NULL)
    return NULL;

  // If we can error out and we have an invoke target, generate an invoke
  // instead of a call.
  if(ast_canerror(ast) && (c->frame->invoke_target != NULL))
    return invoke_fun(c, func, args, i, "", true);

  return codegen_call(c, func, args, i);
}

LLVMValueRef gen_pattern_eq(compile_t* c, ast_t* pattern, LLVMValueRef r_value)
{
  // This is used for structural equality in pattern matching.
  ast_t* pattern_type = ast_type(pattern);
  AST_GET_CHILDREN(pattern_type, package, id);

  // Special case equality on primitive types.
  if(ast_name(package) == c->str_1)
  {
    const char* name = ast_name(id);

    if((name == c->str_Bool) ||
      (name == c->str_I8) ||
      (name == c->str_I16) ||
      (name == c->str_I32) ||
      (name == c->str_I64) ||
      (name == c->str_I128) ||
      (name == c->str_U8) ||
      (name == c->str_U16) ||
      (name == c->str_U32) ||
      (name == c->str_U64) ||
      (name == c->str_U128) ||
      (name == c->str_F32) ||
      (name == c->str_F64)
      )
    {
      return gen_eq_rvalue(c, pattern, r_value);
    }
  }

  // Generate the receiver.
  LLVMValueRef l_value = gen_expr(c, pattern);

  gentype_t g;

  if(!gentype(c, pattern_type, &g))
    return NULL;

  // Static or virtual dispatch.
  LLVMValueRef func = dispatch_function(c, pattern, &g, l_value,
    stringtab("eq"), NULL);

  if(func == NULL)
    return NULL;

  // Call the function. We know it isn't partial.
  LLVMValueRef args[2];
  args[0] = l_value;
  args[1] = r_value;

  // Emit debug location for calls to test for structural equality
  dwarf_location(&c->dwarf, pattern);

  return codegen_call(c, func, args, 2);
}

LLVMValueRef gen_ffi(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, id, typeargs, args);

  // Get the function name, +1 to skip leading @
  const char* f_name = ast_name(id) + 1;

  // Generate the return type.
  ast_t* type = ast_type(ast);
  gentype_t g;

  // Emit dwarf location of ffi call
  dwarf_location(&c->dwarf, ast);

  if(!gentype(c, type, &g))
    return NULL;

  // Get the function.
  LLVMValueRef func = LLVMGetNamedFunction(c->module, f_name);

  if(func == NULL)
  {
    // If we have no prototype, declare one.
    if(!strncmp(f_name, "llvm.", 5))
    {
      // Intrinsic, so use the exact types we supply.
      int count = (int)ast_childcount(args);
      VLA(LLVMTypeRef, f_params, count);
      count = 0;

      ast_t* arg = ast_child(args);

      while(arg != NULL)
      {
        ast_t* p_type = ast_type(arg);
        gentype_t param_g;

        if(!gentype(c, p_type, &param_g))
          return NULL;

        f_params[count++] = param_g.use_type;
        arg = ast_sibling(arg);
      }

      // We may have generated the function by generating a parameter type.
      func = LLVMGetNamedFunction(c->module, f_name);

      if(func == NULL)
      {
        LLVMTypeRef r_type;

        if(g.underlying == TK_TUPLETYPE)
        {
          // Can't use the named type. Build an unnamed type with the same
          // elements.
          int count = LLVMCountStructElementTypes(g.use_type);
          VLA(LLVMTypeRef, e_types, count);
          LLVMGetStructElementTypes(g.use_type, e_types);
          r_type = LLVMStructTypeInContext(c->context, e_types, count, false);
        } else {
          r_type = g.use_type;
        }

        LLVMTypeRef f_type = LLVMFunctionType(r_type, f_params, count,
          false);
        func = LLVMAddFunction(c->module, f_name, f_type);

        if(!ast_canerror(ast))
          LLVMAddFunctionAttr(func, LLVMNoUnwindAttribute);
      }
    } else {
      // Make it varargs.
      LLVMTypeRef f_type = LLVMFunctionType(g.use_type, NULL, 0, true);
      func = LLVMAddFunction(c->module, f_name, f_type);

      if(!ast_canerror(ast))
        LLVMAddFunctionAttr(func, LLVMNoUnwindAttribute);
    }
  }

  // Generate the arguments.
  int count = (int)ast_childcount(args);
  VLA(LLVMValueRef, f_args, count);
  ast_t* arg = ast_child(args);

  for(int i = 0; i < count; i++)
  {
    f_args[i] = gen_expr(c, arg);

    if(f_args[i] == NULL)
      return NULL;

    arg = ast_sibling(arg);
  }

  // If we can error out and we have an invoke target, generate an invoke
  // instead of a call.
  LLVMValueRef result;

  if(ast_canerror(ast) && (c->frame->invoke_target != NULL))
    result = invoke_fun(c, func, f_args, count, "", false);
  else
    result = LLVMBuildCall(c->builder, func, f_args, count, "");

  // Special case a None return value, which is used for void functions.
  if(is_none(type))
    return g.instance;

  return result;
}

LLVMValueRef gencall_runtime(compile_t* c, const char *name,
  LLVMValueRef* args, int count, const char* ret)
{
  // Disable debug anchor
  dwarf_location(&c->dwarf, NULL);

  LLVMValueRef func = LLVMGetNamedFunction(c->module, name);

  if(func == NULL)
    return NULL;

  return LLVMBuildCall(c->builder, func, args, count, ret);
}

LLVMValueRef gencall_create(compile_t* c, gentype_t* g)
{
  // Disable debug anchor
  dwarf_location(&c->dwarf, NULL);

  LLVMValueRef args[1];
  args[0] = LLVMConstBitCast(g->desc, c->descriptor_ptr);

  LLVMValueRef result = gencall_runtime(c, "pony_create", args, 1, "");
  return LLVMBuildBitCast(c->builder, result, g->use_type, "");
}

LLVMValueRef gencall_alloc(compile_t* c, gentype_t* g)
{
  // Do nothing for primitives.
  if(g->primitive != NULL)
    return NULL;

  // Do nothing for Pointer.
  if(is_pointer(g->ast))
    return NULL;

  // Use the global instance if we have one.
  if(g->instance != NULL)
    return g->instance;

  if(g->underlying == TK_ACTOR)
    return gencall_create(c, g);

  return gencall_allocstruct(c, g);
}

LLVMValueRef gencall_allocstruct(compile_t* c, gentype_t* g)
{
  // Disable debug anchor
  dwarf_location(&c->dwarf, NULL);

  // We explicitly want a boxed version.
  // Get the size of the structure.
  size_t size = cast_checked(size_t, LLVMABISizeOfType(c->target_data, g->structure));

  // Get the finaliser, if there is one.
  const char* final = genname_finalise(g->type_name);
  LLVMValueRef final_fun = LLVMGetNamedFunction(c->module, final);

  // Allocate the object.
  LLVMValueRef result;
  LLVMValueRef args[2];
  args[0] = LLVMConstInt(c->i64, size, false);

  if(final_fun == NULL)
  {
    result = gencall_runtime(c, "pony_alloc", args, 1, "");
  } else {
    args[1] = LLVMConstBitCast(final_fun, c->final_fn);
    result = gencall_runtime(c, "pony_alloc_final", args, 2, "");
  }

  result = LLVMBuildBitCast(c->builder, result, g->structure_ptr, "");

  // Set the descriptor.
  LLVMValueRef desc_ptr = LLVMBuildStructGEP(c->builder, result, 0, "");
  LLVMBuildStore(c->builder, g->desc, desc_ptr);

  return result;
}

void gencall_throw(compile_t* c)
{
  // Disable debug location for throw calls
  dwarf_location(&c->dwarf, NULL);

  LLVMValueRef func = LLVMGetNamedFunction(c->module, "pony_throw");

  if(c->frame->invoke_target != NULL)
    invoke_fun(c, func, NULL, 0, "", false);
  else
    LLVMBuildCall(c->builder, func, NULL, 0, "");

  LLVMBuildUnreachable(c->builder);
}
