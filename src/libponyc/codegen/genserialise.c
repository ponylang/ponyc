#include "genserialise.h"
#include "gencall.h"
#include "gendesc.h"
#include "genfun.h"
#include "genname.h"
#include "genopt.h"
#include "genprim.h"
#include "ponyassert.h"
#include "../../libponyrt/mem/pool.h"
#include "../../libponyrt/mem/heap.h"

static void serialise(compile_t* c, reach_type_t* t, LLVMValueRef ctx,
  LLVMValueRef object, LLVMValueRef offset, bool is_bare_tuple)
{
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  LLVMTypeRef structure = c_t->structure;
  int extra = 0;

  switch(t->underlying)
  {
    case TK_PRIMITIVE:
    {
      genserialise_serialiseid(c, t, offset);

      if(c_t->primitive != NULL)
      {
        LLVMValueRef field = LLVMBuildStructGEP2(c->builder, c_t->structure,
          object, 1, "");
        LLVMValueRef f_size = LLVMConstInt(c->intptr,
          LLVMOffsetOfElement(c->target_data, structure, 1), false);
        LLVMValueRef f_offset = LLVMBuildInBoundsGEP2(c->builder, c->i8, offset,
          &f_size, 1, "");

        genserialise_element(c, t, false, ctx, field, f_offset);
      }
      return;
    }

    case TK_CLASS:
    {
      genserialise_serialiseid(c, t, offset);
      extra++;
      break;
    }

    case TK_ACTOR:
    {
      // Skip the actor pad.
      genserialise_serialiseid(c, t, offset);
      extra += 2;
      break;
    }

    case TK_TUPLETYPE:
    {
      if(!is_bare_tuple)
      {
        genserialise_serialiseid(c, t, offset);
        object = LLVMBuildStructGEP2(c->builder, c_t->structure, object, 1, "");
        LLVMValueRef size = LLVMConstInt(c->intptr,
          LLVMOffsetOfElement(c->target_data, structure, 1), false);
        offset = LLVMBuildInBoundsGEP2(c->builder, c->i8, offset, &size, 1, "");
      }

      structure = c_t->primitive;
      break;
    }

    default: {}
  }

  for(uint32_t i = 0; i < t->field_count; i++)
  {
    LLVMValueRef field = LLVMBuildStructGEP2(c->builder, structure, object,
      i + extra, "");
    LLVMValueRef f_size = LLVMConstInt(c->intptr,
      LLVMOffsetOfElement(c->target_data, structure, i + extra), false);
    LLVMValueRef f_offset = LLVMBuildInBoundsGEP2(c->builder, c->i8, offset,
      &f_size, 1, "");

    genserialise_element(c, t->fields[i].type, t->fields[i].embed,
      ctx, field, f_offset);
  }

  if(c_t->custom_serialise_fn != NULL)
  {
    LLVMValueRef f_size = LLVMConstInt(c->intptr,
      LLVMStoreSizeOfType(c->target_data, structure), false);
    LLVMValueRef f_offset = LLVMBuildInBoundsGEP2(c->builder, c->i8, offset,
      &f_size, 1, "");

    LLVMValueRef args[2];

    LLVMTypeRef f_type = LLVMGlobalGetValueType(c_t->custom_serialise_fn);
    size_t buf_size = 2 * sizeof(void *);
    LLVMTypeRef* params = (LLVMTypeRef*)ponyint_pool_alloc_size(buf_size);
    LLVMGetParamTypes(f_type, params);
    args[0] = object;
    args[1] = f_offset;
    codegen_call(c, LLVMGlobalGetValueType(c_t->custom_serialise_fn),
      c_t->custom_serialise_fn, args, 2, true);

    ponyint_pool_free_size(buf_size, params);
  }
}

void genserialise_serialiseid(compile_t* c, reach_type_t* t, LLVMValueRef offset)
{
  // Write the serialise id instead of the descriptor.
  LLVMValueRef value = LLVMConstInt(target_is_ilp32(c->opt->triple) ? c->i32 : c->i64, t->serialise_id, false);
  LLVMBuildStore(c->builder, value, offset);
}

static void serialise_bare_interface(compile_t* c, reach_type_t* t,
  LLVMValueRef ptr, LLVMValueRef offset)
{
  size_t i = HASHMAP_BEGIN;
  reach_type_t* sub = reach_type_cache_next(&t->subtypes, &i);

  if(sub == NULL)
    return;

  LLVMBasicBlockRef current_block = LLVMGetInsertBlock(c->builder);

  LLVMValueRef obj = LLVMBuildLoad2(c->builder, c->ptr, ptr, "");

  LLVMBasicBlockRef post_block = codegen_block(c, "bare_post");
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->intptr, "");

  LLVMPositionBuilderAtEnd(c->builder, current_block);
  reach_type_t* next = reach_type_cache_next(&t->subtypes, &i);

  while(next != NULL)
  {
    LLVMBasicBlockRef next_block = codegen_block(c, "bare_subtype");
    LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntEQ, obj,
      ((compile_type_t*)sub->c_type)->instance, "");
    LLVMBuildCondBr(c->builder, test, post_block, next_block);
    LLVMValueRef value = LLVMConstInt(target_is_ilp32(c->opt->triple) ? c->i32 : c->i64, sub->serialise_id, false);
    LLVMAddIncoming(phi, &value, &current_block, 1);
    LLVMPositionBuilderAtEnd(c->builder, next_block);
    sub = next;
    next = reach_type_cache_next(&t->subtypes, &i);
    current_block = next_block;
  }

  LLVMBuildBr(c->builder, post_block);
  LLVMValueRef value = LLVMConstInt(target_is_ilp32(c->opt->triple) ? c->i32 : c->i64, sub->serialise_id, false);
  LLVMAddIncoming(phi, &value, &current_block, 1);

  LLVMMoveBasicBlockAfter(post_block, current_block);
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMBuildStore(c->builder, phi, offset);
}

void genserialise_element(compile_t* c, reach_type_t* t, bool embed,
  LLVMValueRef ctx, LLVMValueRef ptr, LLVMValueRef offset)
{
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  if(embed || (t->underlying == TK_TUPLETYPE))
  {
    // Embedded field or tuple, serialise in place. Don't load from ptr, as
    // it is already a pointer to the object.
    serialise(c, t, ctx, ptr, offset, true);
  } else if(c_t->primitive != NULL) {
    // Machine word, write the bits to the buffer.
    uint64_t t_align = LLVMABIAlignmentOfType(c->target_data, c_t->mem_type);
    uint64_t align = (t_align < 8) ? t_align : 8;
    LLVMValueRef value = LLVMBuildAlignedLoad(c->builder, c_t->mem_type, ptr, align, "");
    LLVMBuildAlignedStore(c->builder, value, offset, align);
  } else if(t->bare_method != NULL) {
    // Bare object, either write the serialise_id id directly if it is a concrete object
    // or compute the serialise_id id based on the object value and write it if it isn't.
    switch(t->underlying)
    {
      case TK_PRIMITIVE:
        genserialise_serialiseid(c, t, offset);
        break;

      case TK_INTERFACE:
        serialise_bare_interface(c, t, ptr, offset);
        break;

      default:
        pony_assert(false);
        break;
    }
  } else {
    // Lookup the pointer and get the offset, write that.
    LLVMValueRef args[2];
    args[0] = ctx;
    args[1] = LLVMBuildLoad2(c->builder, c->ptr, ptr, "");
    LLVMValueRef object_offset = gencall_runtime(c, "pony_serialise_offset",
      args, 2, "");

    LLVMBuildStore(c->builder, object_offset, offset);
  }
}

static void make_serialise(compile_t* c, reach_type_t* t)
{
  // Use the trace function as the serialise_trace function.
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  c_t->serialise_trace_fn = c_t->trace_fn;

  // Generate the serialise function.
  c_t->serialise_fn = codegen_addfun(c, genname_serialise(t->name),
    c->serialise_fn, true);

  codegen_startfun(c, c_t->serialise_fn, NULL, NULL, NULL, false);
  LLVMSetFunctionCallConv(c_t->serialise_fn, LLVMCCallConv);
  LLVMSetLinkage(c_t->serialise_fn, LLVMExternalLinkage);

  LLVMValueRef ctx = LLVMGetParam(c_t->serialise_fn, 0);
  LLVMValueRef object = LLVMGetParam(c_t->serialise_fn, 1);
  LLVMValueRef addr = LLVMGetParam(c_t->serialise_fn, 2);
  LLVMValueRef offset = LLVMGetParam(c_t->serialise_fn, 3);

  LLVMValueRef offset_addr = LLVMBuildInBoundsGEP2(c->builder, c->i8, addr,
    &offset, 1, "");

  serialise(c, t, ctx, object, offset_addr, false);

  genfun_build_ret_void(c);
  codegen_finishfun(c);
}

static void deserialise(compile_t* c, reach_type_t* t, LLVMValueRef ctx,
  LLVMValueRef object, bool is_bare_tuple)
{
  // The contents have already been copied.
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  LLVMTypeRef structure = c_t->structure;
  int extra = 0;

  switch(t->underlying)
  {
    case TK_PRIMITIVE:
    case TK_CLASS:
    {
      gendeserialise_serialiseid(c, c_t, object);
      extra++;
      break;
    }

    case TK_ACTOR:
    {
      // Skip the actor pad.
      gendeserialise_serialiseid(c, c_t, object);
      extra += 2;
      break;
    }

    case TK_TUPLETYPE:
    {
      if(!is_bare_tuple)
      {
        gendeserialise_serialiseid(c, c_t, object);
        object = LLVMBuildStructGEP2(c->builder, c_t->structure, object, 1, "");
      }

      structure = c_t->primitive;
      break;
    }

    default: {}
  }

  for(uint32_t i = 0; i < t->field_count; i++)
  {
    LLVMValueRef field = LLVMBuildStructGEP2(c->builder, structure, object,
      i + extra, "");
    gendeserialise_element(c, t->fields[i].type, t->fields[i].embed,
      ctx, field);
  }
}

void gendeserialise_serialiseid(compile_t* c, compile_type_t* t, LLVMValueRef object)
{
  // Write the descriptor instead of the serialiseid id.
  LLVMValueRef desc_ptr = LLVMBuildStructGEP2(c->builder, t->structure, object,
    0, "");
  LLVMBuildStore(c->builder, t->desc, desc_ptr);
}

static void deserialise_bare_interface(compile_t* c, LLVMValueRef ptr)
{
  LLVMValueRef args[2];
  args[0] = LLVMConstInt(c->i32, 0, false);
  args[1] = LLVMBuildLoad2(c->builder, c->intptr, ptr, "");

  LLVMValueRef desc = LLVMBuildInBoundsGEP2(c->builder,
    LLVMArrayType(c->ptr, 0), c->desc_table, args, 2, "");
  desc = LLVMBuildLoad2(c->builder, c->ptr, desc, "");
  LLVMValueRef func = gendesc_instance(c, desc);
  LLVMBuildStore(c->builder, func, ptr);
}

void gendeserialise_element(compile_t* c, reach_type_t* t, bool embed,
  LLVMValueRef ctx, LLVMValueRef ptr)
{
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  if(embed || (t->underlying == TK_TUPLETYPE))
  {
    // Embedded field or tuple, deserialise in place.
    deserialise(c, t, ctx, ptr, true);
  } else if(c_t->primitive != NULL) {
    // Machine word, already copied.
  } else if(t->bare_method != NULL){
    // Bare object, either write the function pointer directly if it's a
    // concrete object or look it up in the descriptor table if it isn't.
    switch(t->underlying)
    {
      case TK_PRIMITIVE:
      {
        LLVMBuildStore(c->builder,
          ((compile_method_t*)t->bare_method->c_method)->func, ptr);
        break;
      }

      case TK_INTERFACE:
        deserialise_bare_interface(c, ptr);
        break;

      default:
        pony_assert(false);
        break;
    }
  } else {
    // Lookup the pointer and write that.
    LLVMValueRef args[3];
    args[0] = ctx;
    args[1] = (c_t->desc != NULL) ? c_t->desc : LLVMConstNull(c->ptr);
    args[2] = LLVMBuildLoad2(c->builder, c->intptr, ptr, "");
    LLVMValueRef object = gencall_runtime(c, "pony_deserialise_offset",
      args, 3, "");

    LLVMBuildStore(c->builder, object, ptr);
  }
}

static void make_deserialise(compile_t* c, reach_type_t* t)
{
  // Generate the deserialise function.
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  c_t->deserialise_fn = codegen_addfun(c, genname_deserialise(t->name),
    c->trace_fn, true);

  codegen_startfun(c, c_t->deserialise_fn, NULL, NULL, NULL, false);
  LLVMSetFunctionCallConv(c_t->deserialise_fn, LLVMCCallConv);
  LLVMSetLinkage(c_t->deserialise_fn, LLVMExternalLinkage);

  LLVMValueRef ctx = LLVMGetParam(c_t->deserialise_fn, 0);
  LLVMValueRef object = LLVMGetParam(c_t->deserialise_fn, 1);

  // At this point, the serialised contents have been copied to the allocated
  // object.
  deserialise(c, t, ctx, object, false);

  genfun_build_ret_void(c);
  codegen_finishfun(c);
}

bool genserialise(compile_t* c, reach_type_t* t)
{
  switch(t->underlying)
  {
    case TK_ACTOR:
      // Don't serialise actors.
      return true;

    case TK_TUPLETYPE:
    case TK_PRIMITIVE:
      break;

    case TK_STRUCT:
    {
      // Special case some serialise functions.
      AST_GET_CHILDREN(t->ast, pkg, id);
      const char* package = ast_name(pkg);
      const char* name = ast_name(id);

      if(package == c->str_builtin)
      {
        // Don't serialise NullablePointer[A]
        if(name == c->str_NullablePointer)
          return true;

        // Don't serialise Pointer[A]
        if(name == c->str_Pointer)
          return true;
      }
      break;
    }

    case TK_CLASS:
    {
      // Special case some serialise functions.
      AST_GET_CHILDREN(t->ast, pkg, id);
      const char* package = ast_name(pkg);
      const char* name = ast_name(id);

      if(package == c->str_builtin)
      {
        if(name == c->str_Array)
        {
          genprim_array_serialise_trace(c, t);
          genprim_array_serialise(c, t);
          genprim_array_deserialise(c, t);
          return true;
        }

        if(name == c->str_String)
        {
          genprim_string_serialise_trace(c, t);
          genprim_string_serialise(c, t);
          genprim_string_deserialise(c, t);
          return true;
        }
      }
      break;
    }

    default:
      return true;
  }

  make_serialise(c, t);

  if((t->underlying != TK_PRIMITIVE) ||
    (((compile_type_t*)t->c_type)->primitive != NULL))
    make_deserialise(c, t);

  return true;
}
