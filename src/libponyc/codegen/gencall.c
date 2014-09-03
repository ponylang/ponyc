#include "gencall.h"
#include "gentype.h"
#include "genexpr.h"
#include "genfun.h"
#include "genname.h"
#include "../type/subtype.h"
#include "../type/cap.h"
#include <assert.h>

static LLVMValueRef call_fun(compile_t* c, LLVMValueRef fun, LLVMValueRef* args,
  int count, const char* ret)
{
  if(fun == NULL)
    return NULL;

  return LLVMBuildCall(c->builder, fun, args, count, ret);
}

static LLVMValueRef invoke_fun(compile_t* c, ast_t* try_expr, LLVMValueRef fun,
  LLVMValueRef* args, int count, const char* ret)
{
  if(fun == NULL)
    return NULL;

  LLVMBasicBlockRef this_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef then_block = LLVMInsertBasicBlock(this_block, "then");
  LLVMMoveBasicBlockAfter(then_block, this_block);
  LLVMBasicBlockRef else_block = ast_data(try_expr);

  LLVMValueRef invoke = LLVMBuildInvoke(c->builder, fun, args, count,
    then_block, else_block, ret);

  LLVMPositionBuilderAtEnd(c->builder, then_block);
  return invoke;
}

static LLVMValueRef make_arg(compile_t* c, ast_t* arg, LLVMTypeRef type)
{
  LLVMValueRef value = gen_expr(c, arg);

  if(value == NULL)
    return NULL;

  // TODO: how to determine if the parameter is signed or not
  bool l_sign = false;
  bool r_sign = is_signed(ast_type(arg));

  return gen_assign_cast(c, type, value, l_sign, r_sign);
}

LLVMValueRef gen_call(compile_t* c, ast_t* ast)
{
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

  // dig through function qualification
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

  LLVMValueRef l_value;

  if(need_receiver == 1)
    l_value = gen_expr(c, receiver);

  // static or virtual call?
  const char* method_name = ast_name(method);
  LLVMTypeRef f_type;
  LLVMValueRef func;

  if(g.use_type == c->object_ptr)
  {
    // virtual, get the function by selector colour
    int colour = painter_get_colour(c->painter, method_name);

    // cast the field to a generic object pointer
    l_value = LLVMBuildBitCast(c->builder, l_value, c->object_ptr, "object");

    // get the type descriptor from the object pointer
    LLVMValueRef desc_ptr = LLVMBuildStructGEP(c->builder, l_value, 0, "");
    LLVMValueRef desc = LLVMBuildLoad(c->builder, desc_ptr, "desc");

    // get the function from the vtable
    LLVMValueRef vtable = LLVMBuildStructGEP(c->builder, desc, 7, "");

    LLVMValueRef index[2];
    index[0] = LLVMConstInt(LLVMInt32Type(), 0, false);
    index[1] = LLVMConstInt(LLVMInt32Type(), colour, false);

    LLVMValueRef func_ptr = LLVMBuildGEP(c->builder, vtable, index, 2, "");
    func = LLVMBuildLoad(c->builder, func_ptr, "");

    // TODO: What if the function signature takes a primitive but the real
    // underlying function accepts a union type of that primitive with something
    // else, and so requires a boxed primitive? Or the real function could take
    // a trait that the primitive provides.

    // cast to the right function type
    LLVMValueRef proto = genfun_proto(c, &g, method_name, typeargs);

    if(proto == NULL)
    {
      ast_error(ast, "couldn't locate '%s'", method_name);
      return NULL;
    }

    f_type = LLVMTypeOf(proto);
    func = LLVMBuildBitCast(c->builder, func, f_type, "method");
  } else {
    // static, get the actual function
    func = genfun_proto(c, &g, method_name, typeargs);

    if(func == NULL)
    {
      ast_error(ast, "couldn't locate '%s'", method_name);
      return NULL;
    }

    f_type = LLVMTypeOf(func);
  }

  int count = ast_childcount(positional) + need_receiver;

  LLVMValueRef args[count];
  LLVMTypeRef params[count];
  LLVMGetParamTypes(LLVMGetElementType(f_type), params);

  if(need_receiver == 1)
  {
    LLVMValueRef value = make_arg(c, receiver, params[0]);

    if(value == NULL)
      return NULL;

    args[0] = value;
  }

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
      return invoke_fun(c, try_expr, func, args, count, "");
  }

  return call_fun(c, func, args, count, "");
}

LLVMValueRef gencall_runtime(compile_t* c, const char *name,
  LLVMValueRef* args, int count, const char* ret)
{
  return call_fun(c, LLVMGetNamedFunction(c->module, name), args, count, ret);
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
  size_t size = LLVMABISizeOfType(c->target, l_type);

  LLVMValueRef args[1];
  args[0] = LLVMConstInt(LLVMInt64Type(), size, false);

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
  // cast the field to an object pointer
  LLVMValueRef args[2];
  args[0] = LLVMBuildBitCast(c->builder, value, c->object_ptr, "object");

  // get the type descriptor from the object pointer
  LLVMValueRef desc_ptr = LLVMBuildStructGEP(c->builder, args[0], 0, "");
  LLVMValueRef desc = LLVMBuildLoad(c->builder, desc_ptr, "desc");

  // determine if this is an actor or not
  LLVMValueRef dispatch_ptr = LLVMBuildStructGEP(c->builder, desc, 3, "");
  LLVMValueRef dispatch = LLVMBuildLoad(c->builder, dispatch_ptr, "dispatch");
  LLVMValueRef is_object = LLVMBuildIsNull(c->builder, dispatch, "is_object");

  // build a conditional
  LLVMValueRef fun = LLVMGetBasicBlockParent(LLVMGetInsertBlock(c->builder));
  LLVMBasicBlockRef then_block = LLVMAppendBasicBlock(fun, "then");
  LLVMBasicBlockRef else_block = LLVMAppendBasicBlock(fun, "else");
  LLVMBasicBlockRef merge_block = LLVMAppendBasicBlock(fun, "merge");

  LLVMBuildCondBr(c->builder, is_object, then_block, else_block);

  // if we're an object
  LLVMPositionBuilderAtEnd(c->builder, then_block);

  // get the trace function from the type descriptor
  LLVMValueRef trace_ptr = LLVMBuildStructGEP(c->builder, desc, 0, "");
  args[1] = LLVMBuildLoad(c->builder, trace_ptr, "trace");

  gencall_runtime(c, "pony_traceobject", args, 2, "");
  LLVMBuildBr(c->builder, merge_block);

  // if we're an actor
  LLVMPositionBuilderAtEnd(c->builder, else_block);
  gencall_runtime(c, "pony_traceactor", args, 1, "");
  LLVMBuildBr(c->builder, merge_block);

  // continue in the merge block
  LLVMPositionBuilderAtEnd(c->builder, merge_block);
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
      trace_known(c, value, genname_type(type));
      return true;

    case TK_NOMINAL:
    {
      bool tag = cap_for_type(type) == TK_TAG;

      switch(ast_id(ast_data(type)))
      {
        case TK_TRAIT:
          if(tag)
            trace_tag(c, value);
          else
            trace_unknown(c, value);

          return true;

        case TK_DATA:
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
