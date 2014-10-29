#include "gencall.h"
#include "genoperator.h"
#include "gentype.h"
#include "genexpr.h"
#include "gendesc.h"
#include "genfun.h"
#include "genname.h"
#include "../pkg/platformfuns.h"
#include "../type/subtype.h"
#include "../type/cap.h"
#include "../ds/stringtab.h"
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

  if(fastcc)
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
  AST_GET_CHILDREN(ast, postfix, positional, named);
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
  AST_GET_CHILDREN(ast, postfix, positional, named);
  AST_GET_CHILDREN(postfix, receiver, method);

  const char* method_name = ast_name(method);
  bool is_target;

  if(os_is_target(method_name, c->release, &is_target))
    return LLVMConstInt(c->i1, is_target ? 1 : 0, false);

  ast_error(ast, "unknown Platform setting");
  return NULL;
}

static bool special_case_call(compile_t* c, ast_t* ast, LLVMValueRef* value)
{
  AST_GET_CHILDREN(ast, postfix, positional, named);

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
    os_is_target(OS_HAS_I128_NAME, c->release, &has_i128);
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
    int colour = painter_get_colour(c->painter, method_name);

    // Get the function from the vtable.
    func = gendesc_vtable(c, l_value, colour);

    // Cast to the right function type.
    LLVMValueRef proto = genfun_proto(c, g, method_name, typeargs);

    if(proto == NULL)
    {
      ast_error(from, "couldn't locate '%s'", method_name);
      return NULL;
    }

    LLVMTypeRef f_type = LLVMTypeOf(proto);
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

  AST_GET_CHILDREN(ast, postfix, positional, named);
  AST_GET_CHILDREN(postfix, receiver, method);
  ast_t* typeargs = NULL;

  // Dig through function qualification.
  if(ast_id(receiver) == TK_FUNREF)
  {
    AST_GET_CHILDREN_NO_DECL(receiver, receiver, typeargs);
  }

  if(typeargs != NULL)
  {
    ast_error(typeargs,
      "not implemented (codegen for polymorphic methods)");
    return NULL;
  }

  ast_t* type = ast_type(receiver);
  const char* method_name = ast_name(method);

  // Generate the receiver type.
  gentype_t g;

  if(!gentype(c, type, &g))
    return NULL;

  bool need_receiver = call_needs_receiver(postfix, &g);

  // Generate the arguments.
  LLVMValueRef proto = genfun_proto(c, &g, method_name, typeargs);

  if(proto == NULL)
  {
    ast_error(ast, "couldn't locate '%s'", method_name);
    return NULL;
  }

  LLVMTypeRef f_type = LLVMTypeOf(proto);
  size_t count = ast_childcount(positional) + need_receiver;

  VLA(LLVMValueRef, args, count);
  VLA(LLVMTypeRef, params, count);
  LLVMGetParamTypes(LLVMGetElementType(f_type), params);

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
        args[0] = gencall_alloc(c, &g);
        break;

      case TK_NEWBEREF:
        args[0] = gencall_create(c, &g);
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

  if(!gentype(c, type, &g))
    return NULL;

  // Get the function.
  LLVMValueRef func = LLVMGetNamedFunction(c->module, f_name);

  if(func == NULL)
  {
    // If we have no prototype, make an external vararg function.
    LLVMTypeRef f_type = LLVMFunctionType(g.use_type, NULL, 0, true);
    func = LLVMAddFunction(c->module, f_name, f_type);
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

  // Call it.
  LLVMValueRef result = LLVMBuildCall(c->builder, func, f_args, count, "");

  // Special case a None return value, which is used for void functions.
  if(is_none(type))
    return g.instance;

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

LLVMValueRef gencall_create(compile_t* c, gentype_t* g)
{
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

  return gencall_allocstruct(c, g);
}

LLVMValueRef gencall_allocstruct(compile_t* c, gentype_t* g)
{
  // We explicitly want a boxed version.
  // Get the size of the structure.
  size_t size = LLVMABISizeOfType(c->target_data, g->structure);

  // Allocate the object.
  LLVMValueRef args[1];
  args[0] = LLVMConstInt(c->i64, size, false);

  LLVMValueRef result = gencall_runtime(c, "pony_alloc", args, 1, "");
  result = LLVMBuildBitCast(c->builder, result, g->structure_ptr, "");

  // Set the descriptor.
  LLVMValueRef desc_ptr = LLVMBuildStructGEP(c->builder, result, 0, "");
  LLVMBuildStore(c->builder, g->desc, desc_ptr);

  return result;
}

static void trace_tag(compile_t* c, LLVMValueRef value)
{
  // cast the value to a void pointer
  LLVMValueRef args[1];
  args[0] = LLVMBuildBitCast(c->builder, value, c->void_ptr, "");

  gencall_runtime(c, "pony_trace", args, 1, "");
}

static void trace_actor(compile_t* c, LLVMValueRef value)
{
  // cast the value to an object pointer
  LLVMValueRef args[1];
  args[0] = LLVMBuildBitCast(c->builder, value, c->object_ptr, "");

  gencall_runtime(c, "pony_traceactor", args, 1, "");
}

static bool trace_known(compile_t* c, LLVMValueRef value, ast_t* type)
{
  gentype_t g;

  if(!gentype(c, type, &g))
    return false;

  // get the trace function statically
  const char* fun = genname_trace(g.type_name);

  LLVMValueRef args[2];
  args[1] = LLVMGetNamedFunction(c->module, fun);

  // if this type has no trace function, don't try to recurse in the runtime
  if(args[1] != NULL)
  {
    // cast the value to an object pointer
    args[0] = LLVMBuildBitCast(c->builder, value, c->object_ptr, "");
    gencall_runtime(c, "pony_traceobject", args, 2, "");
  } else {
    // cast the value to a void pointer
    args[0] = LLVMBuildBitCast(c->builder, value, c->void_ptr, "");
    gencall_runtime(c, "pony_trace", args, 1, "");
  }

  return true;
}

static void trace_unknown(compile_t* c, LLVMValueRef value)
{
  // Determine if this is an actor or not.
  LLVMValueRef dispatch = gendesc_dispatch(c, value);
  LLVMValueRef is_object = LLVMBuildIsNull(c->builder, dispatch, "is_object");

  // Build a conditional.
  LLVMBasicBlockRef then_block = codegen_block(c, "trace_then");
  LLVMBasicBlockRef else_block = codegen_block(c, "trace_else");
  LLVMBasicBlockRef post_block = codegen_block(c, "trace_post");

  LLVMBuildCondBr(c->builder, is_object, then_block, else_block);

  // We're an object.
  LLVMPositionBuilderAtEnd(c->builder, then_block);

  // Get the trace function from the type descriptor.
  LLVMValueRef args[2];
  args[0] = value;
  args[1] = gendesc_trace(c, value);

  gencall_runtime(c, "pony_traceobject", args, 2, "");
  LLVMBuildBr(c->builder, post_block);

  // We're an actor.
  LLVMPositionBuilderAtEnd(c->builder, else_block);
  gencall_runtime(c, "pony_traceactor", args, 1, "");
  LLVMBuildBr(c->builder, post_block);

  // Continue in the post block.
  LLVMPositionBuilderAtEnd(c->builder, post_block);
}

static bool trace_tuple(compile_t* c, LLVMValueRef value, ast_t* type)
{
  // Invoke the trace function directly. Do not trace the address of the tuple.
  const char* type_name = genname_type(type);
  const char* trace_name = genname_tracetuple(type_name);
  LLVMValueRef trace_fn = LLVMGetNamedFunction(c->module, trace_name);

  // There will be no trace function if the tuple doesn't need tracing.
  if(trace_fn == NULL)
    return false;

  return (LLVMBuildCall(c->builder, trace_fn, &value, 1, "") != NULL);
}

bool gencall_trace(compile_t* c, LLVMValueRef value, ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    {
      // TODO: this currently crashes if there is a tuple in the union
      bool tag = cap_for_type(type) == TK_TAG;

      if(tag)
      {
        // TODO: Check our underlying type. If, in the union, that underlying
        // type could be a tag, trace this as a tag. Otherwise, trace this as
        // an unknown.
        trace_tag(c, value);
      } else {
        // This union type can never be a tag.
        trace_unknown(c, value);
      }

      return true;
    }

    case TK_TUPLETYPE:
      return trace_tuple(c, value, type);

    case TK_NOMINAL:
    {
      bool tag = cap_for_type(type) == TK_TAG;

      switch(ast_id((ast_t*)ast_data(type)))
      {
        case TK_INTERFACE:
        case TK_TRAIT:
          if(tag)
            trace_tag(c, value);
          else
            trace_unknown(c, value);

          return true;

        case TK_PRIMITIVE:
          // do nothing
          return false;

        case TK_CLASS:
          if(tag)
          {
            trace_tag(c, value);
            return true;
          }

          return trace_known(c, value, type);

        case TK_ACTOR:
          trace_actor(c, value);
          return true;

        default: {}
      }
      break;
    }

    case TK_ISECTTYPE:
    {
      // TODO: this currently crashes if there is a tuple in the isect
      bool tag = cap_for_type(type) == TK_TAG;

      if(tag)
        trace_tag(c, value);
      else
        trace_unknown(c, value);

      return true;
    }

    default: {}
  }

  assert(0);
  return false;
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
