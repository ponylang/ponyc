#include "gencall.h"
#include "gentype.h"
#include "genexpr.h"
#include "gendesc.h"
#include "genfun.h"
#include "genname.h"
#include "../pkg/platformfuns.h"
#include "../type/subtype.h"
#include "../type/cap.h"
#include <string.h>
#include <assert.h>

static LLVMValueRef invoke_fun(compile_t* c, ast_t* try_expr, LLVMValueRef fun,
  LLVMValueRef* args, int count, const char* ret, bool fastcc)
{
  if(fun == NULL)
    return NULL;

  LLVMBasicBlockRef this_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef then_block = LLVMInsertBasicBlockInContext(c->context,
    this_block, "then");
  LLVMMoveBasicBlockAfter(then_block, this_block);
  LLVMBasicBlockRef else_block = (LLVMBasicBlockRef)ast_data(try_expr);

  LLVMValueRef invoke = LLVMBuildInvoke(c->builder, fun, args, count,
    then_block, else_block, ret);

  if(fastcc)
    LLVMSetInstructionCallConv(invoke, LLVMFastCallConv);

  LLVMPositionBuilderAtEnd(c->builder, then_block);
  return invoke;
}

static LLVMValueRef make_arg(compile_t* c, ast_t* arg, LLVMTypeRef type)
{
  LLVMValueRef value = gen_expr(c, arg);

  if(value == NULL)
    return NULL;

  return gen_assign_cast(c, type, value, ast_type(arg));
}

static LLVMValueRef make_platform_call(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, postfix, positional, named);

  if((ast_id(postfix) != TK_FUNREF) ||
    (ast_id(positional) != TK_NONE) ||
    (ast_id(named) != TK_NONE)
    )
    return NULL;

  AST_GET_CHILDREN(postfix, receiver, method);
  ast_t* receiver_type = ast_type(receiver);

  if(ast_id(receiver_type) != TK_NOMINAL)
    return NULL;

  ast_t* def = (ast_t*)ast_data(receiver_type);
  ast_t* id = ast_child(def);
  const char* name = ast_name(id);

  if(strcmp(name, "Platform"))
    return NULL;

  const char* method_name = ast_name(method);

  bool is_target;
  if(os_is_target(method_name, c->release, &is_target))
    return LLVMConstInt(c->i1, is_target ? 1 : 0, false);

  return NULL;
}

LLVMValueRef gen_call(compile_t* c, ast_t* ast)
{
  // Special case Platform calls.
  LLVMValueRef platform = make_platform_call(c, ast);

  if(platform != NULL)
    return platform;

  AST_GET_CHILDREN(ast, postfix, positional, named);

  int need_receiver;

  switch(ast_id(postfix))
  {
    case TK_NEWREF:
    case TK_NEWBEREF:
      need_receiver = 0;
      break;

    case TK_BEREF:
    case TK_FUNREF:
      need_receiver = 1;
      break;

    default:
      assert(0);
      return NULL;
  }

  ast_t* typeargs = NULL;
  AST_GET_CHILDREN(postfix, receiver, method);

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
  gentype_t g;

  if(!gentype(c, type, &g))
    return NULL;

  LLVMValueRef l_value = NULL;

  if(need_receiver == 1)
    l_value = gen_expr(c, receiver);

  // Static or virtual call?
  const char* method_name = ast_name(method);
  LLVMTypeRef f_type;
  LLVMValueRef func;

  if(g.use_type == c->object_ptr)
  {
    // Virtual, get the function by selector colour.
    int colour = painter_get_colour(c->painter, method_name);

    // Get the function from the vtable.
    func = gendesc_vtable(c, l_value, colour);

    // TODO: What if the function signature takes a primitive but the real
    // underlying function accepts a union type of that primitive with something
    // else, and so requires a boxed primitive? Or the real function could take
    // a trait that the primitive provides.

    // Cast to the right function type.
    LLVMValueRef proto = genfun_proto(c, &g, method_name, typeargs);

    if(proto == NULL)
    {
      ast_error(ast, "couldn't locate '%s'", method_name);
      return NULL;
    }

    f_type = LLVMTypeOf(proto);
    func = LLVMBuildBitCast(c->builder, func, f_type, "method");
  } else {
    // Static, get the actual function.
    func = genfun_proto(c, &g, method_name, typeargs);

    if(func == NULL)
    {
      ast_error(ast, "couldn't locate '%s'", method_name);
      return NULL;
    }

    f_type = LLVMTypeOf(func);
  }

  size_t count = ast_childcount(positional) + need_receiver;

  VLA(LLVMValueRef, args, count);
  VLA(LLVMTypeRef, params, count);
  LLVMGetParamTypes(LLVMGetElementType(f_type), params);

  if(need_receiver == 1)
    args[0] = l_value;

  ast_t* arg = ast_child(positional);

  for(int i = need_receiver; i < count; i++)
  {
    LLVMValueRef value = make_arg(c, arg, params[i]);

    if(value == NULL)
      return NULL;

    args[i] = value;
    arg = ast_sibling(arg);
  }

  if(ast_canerror(ast))
  {
    // If we can error out and we're called in the body of a try expression,
    // generate an invoke instead of a call.
    size_t clause;
    ast_t* try_expr = ast_enclosing_try(ast, &clause);

    if((try_expr != NULL) && (clause == 0))
      return invoke_fun(c, try_expr, func, args, (int)count, "", true);
  }

  LLVMValueRef result = LLVMBuildCall(c->builder, func, args,
    (unsigned int)count, "");

  LLVMSetInstructionCallConv(result, LLVMFastCallConv);
  return result;
}

LLVMValueRef gen_ffi(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, id, typeargs, args);

  // Get the function name.
  const char* f_name = ast_name(id);

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
  size_t count = ast_childcount(args);
  VLA(LLVMValueRef, f_args, count);
  ast_t* arg = ast_child(args);

  for(size_t i = 0; i < count; i++)
  {
    f_args[i] = gen_expr(c, arg);

    if(f_args[i] == NULL)
      return NULL;

    arg = ast_sibling(arg);
  }

  // Call it.
  return LLVMBuildCall(c->builder, func, f_args, (unsigned int)count, "");
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

LLVMValueRef gencall_alloc(compile_t* c, LLVMTypeRef type)
{
  LLVMTypeRef l_type = LLVMGetElementType(type);
  size_t size = LLVMABISizeOfType(c->target_data, l_type);

  LLVMValueRef args[1];
  args[0] = LLVMConstInt(c->i64, size, false);

  LLVMValueRef result = gencall_runtime(c, "pony_alloc", args, 1, "");
  return LLVMBuildBitCast(c->builder, result, type, "");
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

static void trace_known(compile_t* c, LLVMValueRef value, const char* name)
{
  // get the trace function statically
  const char* fun = genname_trace(name);

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
}

static void trace_unknown(compile_t* c, LLVMValueRef value)
{
  // determine if this is an actor or not
  LLVMValueRef dispatch = gendesc_dispatch(c, value);
  LLVMValueRef is_object = LLVMBuildIsNull(c->builder, dispatch, "is_object");

  // build a conditional
  LLVMBasicBlockRef then_block = codegen_block(c, "then");
  LLVMBasicBlockRef else_block = codegen_block(c, "else");
  LLVMBasicBlockRef merge_block = codegen_block(c, "merge");

  LLVMBuildCondBr(c->builder, is_object, then_block, else_block);

  // if we're an object
  LLVMPositionBuilderAtEnd(c->builder, then_block);

  // get the trace function from the type descriptor
  LLVMValueRef args[2];
  args[0] = value;
  args[1] = gendesc_trace(c, value);

  gencall_runtime(c, "pony_traceobject", args, 2, "");
  LLVMBuildBr(c->builder, merge_block);

  // if we're an actor
  LLVMPositionBuilderAtEnd(c->builder, else_block);
  gencall_runtime(c, "pony_traceactor", args, 1, "");
  LLVMBuildBr(c->builder, merge_block);

  // continue in the merge block
  LLVMPositionBuilderAtEnd(c->builder, merge_block);
}

bool trace_tuple(compile_t* c, LLVMValueRef value, ast_t* type)
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
      if(!is_bool(type))
      {
        bool tag = cap_for_type(type) == TK_TAG;

        if(tag)
        {
          // TODO: are we really a tag? need runtime info
          trace_tag(c, value);
        } else {
          // this union type can never be a tag
          trace_unknown(c, value);
        }
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
            trace_tag(c, value);
          else
            trace_known(c, value, genname_type(type));

          return true;

        case TK_ACTOR:
          trace_actor(c, value);
          return true;

        default: {}
      }
      break;
    }

    case TK_ISECTTYPE:
    case TK_STRUCTURAL:
    {
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

void gencall_throw(compile_t* c, ast_t* try_expr)
{
  LLVMValueRef func = LLVMGetNamedFunction(c->module, "pony_throw");

  if(try_expr != NULL)
    invoke_fun(c, try_expr, func, NULL, 0, "", false);
  else
    LLVMBuildCall(c->builder, func, NULL, 0, "");

  LLVMBuildUnreachable(c->builder);
}
