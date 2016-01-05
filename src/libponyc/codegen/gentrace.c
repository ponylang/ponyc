#include "gentrace.h"
#include "gencall.h"
#include "gendesc.h"
#include "genname.h"
#include "genprim.h"
#include "../type/cap.h"
#include "../type/subtype.h"
#include <assert.h>

typedef enum
{
  TRACE_NONE,
  TRACE_TAG,
  TRACE_MAYBE_TAG,
  TRACE_NOT_TAG
} trace_tag_t;

static trace_tag_t trace_as_tag(compile_t* c, ast_t* parent, ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    {
      ast_t* child = ast_child(type);
      trace_tag_t t = TRACE_NONE;

      while(child != NULL)
      {
        switch(trace_as_tag(c, parent, child))
        {
          case TRACE_NONE:
            // Primitives don't need to be traced.
            break;

          case TRACE_TAG:
            // If a union has a tag with anything else, it's a "maybe tag".
            if(t == TRACE_NONE)
              t = TRACE_TAG;

            if(t != TRACE_TAG)
              return TRACE_MAYBE_TAG;
            break;

          case TRACE_MAYBE_TAG:
            // If a union has a "maybe tag", it's a "maybe tag".
            return TRACE_MAYBE_TAG;

          case TRACE_NOT_TAG:
            // If a union has a "not tag" with anything else, it's a
            // "maybe tag".
            if(t == TRACE_NONE)
              t = TRACE_NOT_TAG;

            if(t != TRACE_NOT_TAG)
              return TRACE_MAYBE_TAG;
            break;
        }

        child = ast_sibling(child);
      }

      // The union is either all tag or all "not tag".
      return t;
    }

    case TK_ISECTTYPE:
    {
      ast_t* child = ast_child(type);
      trace_tag_t t = TRACE_NONE;

      while(child != NULL)
      {
        switch(trace_as_tag(c, parent, child))
        {
          case TRACE_NONE:
            // Primitives don't need to be traced.
            break;

          case TRACE_TAG:
            // If an isect has only tag elements, it's a tag.
            if(t == TRACE_NONE)
              t = TRACE_TAG;
            break;

          case TRACE_MAYBE_TAG:
            // If an isect has any "maybe tag" elements, and not "not tag"
            // elements, it's a "maybe tag".
            t = TRACE_MAYBE_TAG;
            break;

          case TRACE_NOT_TAG:
            // If an isect has any "not tag" elements, it's not a tag.
            return TRACE_NOT_TAG;
        }

        child = ast_sibling(child);
      }

      return t;
    }

    case TK_TUPLETYPE:
      return TRACE_NOT_TAG;

    case TK_NOMINAL:
      // Primitives don't need to be traced.
      if(ast_id((ast_t*)ast_data(type)) == TK_PRIMITIVE)
        return TRACE_NONE;

      if(cap_single(type) == TK_TAG)
        return TRACE_TAG;

      return TRACE_NOT_TAG;

    default: {}
  }

  assert(0);
  return TRACE_NOT_TAG;
}

static void trace_tag(compile_t* c, LLVMValueRef ctx, LLVMValueRef value)
{
  // Cast the value to a void pointer.
  LLVMValueRef args[2];
  args[0] = ctx;
  args[1] = LLVMBuildBitCast(c->builder, value, c->void_ptr, "");

  gencall_runtime(c, "pony_trace", args, 2, "");
}

static void trace_tag_or_actor(compile_t* c, LLVMValueRef ctx,
  LLVMValueRef value)
{
  // We're an object.
  LLVMValueRef args[2];
  args[0] = ctx;
  args[1] = value;
  gencall_runtime(c, "pony_trace_tag_or_actor", args, 2, "");
}

static void trace_actor(compile_t* c, LLVMValueRef ctx, LLVMValueRef value)
{
  // Cast the value to an object pointer.
  LLVMValueRef args[2];
  args[0] = ctx;
  args[1] = LLVMBuildBitCast(c->builder, value, c->object_ptr, "");

  gencall_runtime(c, "pony_traceactor", args, 2, "");
}

static void trace_maybe(compile_t* c, LLVMValueRef ctx, LLVMValueRef value,
  ast_t* type, bool tag)
{
  ast_t* type_args = ast_childidx(type, 2);
  ast_t* elem = ast_child(type_args);

  if(is_machine_word(elem))
    return;

  LLVMValueRef test = genprim_maybe_is_null(c, elem, value);
  LLVMBasicBlockRef is_false = codegen_block(c, "");
  LLVMBasicBlockRef is_true = codegen_block(c, "");
  LLVMBuildCondBr(c->builder, test, is_true, is_false);

  LLVMPositionBuilderAtEnd(c->builder, is_false);

  if(tag)
    trace_tag(c, ctx, value);
  else
    gentrace(c, ctx, value, elem);

  LLVMBuildBr(c->builder, is_true);
  LLVMPositionBuilderAtEnd(c->builder, is_true);
}

static bool trace_known(compile_t* c, LLVMValueRef ctx, LLVMValueRef value,
  ast_t* type)
{
  gentype_t g;

  if(!gentype(c, type, &g))
    return false;

  // Get the trace function statically.
  const char* fun = genname_trace(g.type_name);
  LLVMValueRef trace_fn = LLVMGetNamedFunction(c->module, fun);

  // If this type has no trace function, don't try to recurse in the runtime.
  if(trace_fn != NULL)
  {
    // Cast the value to an object pointer.
    LLVMValueRef args[3];
    args[0] = ctx;
    args[1] = LLVMBuildBitCast(c->builder, value, c->object_ptr, "");
    args[2] = trace_fn;

    gencall_runtime(c, "pony_traceobject", args, 3, "");
  } else {
    // Cast the value to a void pointer.
    LLVMValueRef args[2];
    args[0] = ctx;
    args[1] = LLVMBuildBitCast(c->builder, value, c->void_ptr, "");
    gencall_runtime(c, "pony_trace", args, 2, "");
  }

  return true;
}

static void trace_unknown(compile_t* c, LLVMValueRef ctx, LLVMValueRef value)
{
  // We're an object.
  LLVMValueRef args[2];
  args[0] = ctx;
  args[1] = value;

  gencall_runtime(c, "pony_traceunknown", args, 2, "");
}

static bool trace_tuple(compile_t* c, LLVMValueRef ctx, LLVMValueRef value,
  ast_t* type)
{
  // Invoke the trace function directly. Do not trace the address of the tuple.
  const char* type_name = genname_type(type);
  const char* trace_name = genname_tracetuple(type_name);
  LLVMValueRef trace_fn = LLVMGetNamedFunction(c->module, trace_name);

  // There will be no trace function if the tuple doesn't need tracing.
  if(trace_fn == NULL)
    return false;

  LLVMValueRef args[2];
  args[0] = ctx;
  args[1] = value;

  LLVMBuildCall(c->builder, trace_fn, args, 2, "");
  return true;
}

bool gentrace(compile_t* c, LLVMValueRef ctx, LLVMValueRef value, ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      switch(trace_as_tag(c, type, type))
      {
        case TRACE_NONE:
          return false;

        case TRACE_TAG:
          trace_tag_or_actor(c, ctx, value);
          break;

        case TRACE_MAYBE_TAG:
          assert(0);
          break;

        case TRACE_NOT_TAG:
          trace_unknown(c, ctx, value);
          break;
      }

      return true;
    }

    case TK_TUPLETYPE:
      return trace_tuple(c, ctx, value, type);

    case TK_NOMINAL:
    {
      switch(ast_id((ast_t*)ast_data(type)))
      {
        case TK_INTERFACE:
        case TK_TRAIT:
          if(cap_single(type) == TK_TAG)
            trace_tag_or_actor(c, ctx, value);
          else
            trace_unknown(c, ctx, value);

          return true;

        case TK_PRIMITIVE:
          // Do nothing.
          return false;

        case TK_STRUCT:
        case TK_CLASS:
          if(cap_single(type) == TK_TAG)
          {
            if(is_maybe(type))
              trace_maybe(c, ctx, value, type, true);
            else
              trace_tag(c, ctx, value);

            return true;
          }

          if(is_maybe(type))
          {
            trace_maybe(c, ctx, value, type, false);
            return true;
          }

          return trace_known(c, ctx, value, type);

        case TK_ACTOR:
          trace_actor(c, ctx, value);
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
