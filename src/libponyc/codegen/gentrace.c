#include "gentrace.h"
#include "gencall.h"
#include "gendesc.h"
#include "genname.h"
#include "../type/cap.h"
#include <assert.h>

static bool trace_as_tag(compile_t* c, LLVMValueRef value, ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    {
      // TODO: complicated due to disjoint sets.
      return false;
    }

    case TK_ISECTTYPE:
    {
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        if(!trace_as_tag(c, value, child))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_TUPLETYPE:
      return false;

    case TK_NOMINAL:
      return cap_single(type) == TK_TAG;

    default: {}
  }

  assert(0);
  return false;
}

static void trace_tag(compile_t* c, LLVMValueRef value)
{
  // Cast the value to a void pointer.
  LLVMValueRef args[1];
  args[0] = LLVMBuildBitCast(c->builder, value, c->void_ptr, "");

  gencall_runtime(c, "pony_trace", args, 1, "");
}

static void trace_tag_or_actor(compile_t* c, LLVMValueRef value)
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
  LLVMValueRef args[1];
  args[0] = value;

  gencall_runtime(c, "pony_trace", args, 1, "");
  LLVMBuildBr(c->builder, post_block);

  // We're an actor.
  LLVMPositionBuilderAtEnd(c->builder, else_block);
  gencall_runtime(c, "pony_traceactor", args, 1, "");
  LLVMBuildBr(c->builder, post_block);

  // Continue in the post block.
  LLVMPositionBuilderAtEnd(c->builder, post_block);
}

static void trace_actor(compile_t* c, LLVMValueRef value)
{
  // Cast the value to an object pointer.
  LLVMValueRef args[1];
  args[0] = LLVMBuildBitCast(c->builder, value, c->object_ptr, "");

  gencall_runtime(c, "pony_traceactor", args, 1, "");
}

static bool trace_known(compile_t* c, LLVMValueRef value, ast_t* type)
{
  gentype_t g;

  if(!gentype(c, type, &g))
    return false;

  // Get the trace function statically.
  const char* fun = genname_trace(g.type_name);

  LLVMValueRef args[2];
  args[1] = LLVMGetNamedFunction(c->module, fun);

  // If this type has no trace function, don't try to recurse in the runtime.
  if(args[1] != NULL)
  {
    // Cast the value to an object pointer.
    args[0] = LLVMBuildBitCast(c->builder, value, c->object_ptr, "");
    gencall_runtime(c, "pony_traceobject", args, 2, "");
  } else {
    // Cast the value to a void pointer.
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

bool gentrace(compile_t* c, LLVMValueRef value, ast_t* type)
{
  bool tag = trace_as_tag(c, value, type);

  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      if(tag)
        trace_tag_or_actor(c, value);
      else
        trace_unknown(c, value);

      return true;
    }

    case TK_TUPLETYPE:
      return trace_tuple(c, value, type);

    case TK_NOMINAL:
    {
      switch(ast_id((ast_t*)ast_data(type)))
      {
        case TK_INTERFACE:
        case TK_TRAIT:
          if(tag)
            trace_tag_or_actor(c, value);
          else
            trace_unknown(c, value);

          return true;

        case TK_PRIMITIVE:
          // Do nothing.
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

    default: {}
  }

  assert(0);
  return false;
}
