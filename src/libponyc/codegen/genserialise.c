#include "genserialise.h"
#include "genprim.h"
#include "genname.h"
#include "gencall.h"

static void serialise(compile_t* c, reach_type_t* t, LLVMValueRef ctx,
  LLVMValueRef object, LLVMValueRef offset)
{
  LLVMTypeRef structure = t->structure;
  int extra = 0;

  switch(t->underlying)
  {
    case TK_PRIMITIVE:
    {
      genserialise_typeid(c, t, offset);

      if(t->primitive != NULL)
      {
        LLVMValueRef field = LLVMBuildStructGEP(c->builder, object, 1, "");
        LLVMValueRef f_offset = LLVMBuildAdd(c->builder, offset,
          LLVMConstInt(c->intptr,
            LLVMOffsetOfElement(c->target_data, structure, 1), false), "");

        genserialise_element(c, t, false, ctx, field, f_offset);
      }
      return;
    }

    case TK_CLASS:
    {
      genserialise_typeid(c, t, offset);
      extra++;
      break;
    }

    case TK_ACTOR:
    {
      // Skip the actor pad.
      genserialise_typeid(c, t, offset);
      extra += 2;
      break;
    }

    case TK_TUPLETYPE:
    {
      // Get the tuple primitive type.
      if(LLVMTypeOf(object) == t->structure_ptr)
      {
        genserialise_typeid(c, t, offset);
        object = LLVMBuildStructGEP(c->builder, object, 1, "");
        offset = LLVMBuildAdd(c->builder, offset,
          LLVMConstInt(c->intptr,
            LLVMOffsetOfElement(c->target_data, structure, 1), false), "");
      }

      structure = t->primitive;
      break;
    }

    default: {}
  }

  for(uint32_t i = 0; i < t->field_count; i++)
  {
    LLVMValueRef field = LLVMBuildStructGEP(c->builder, object, i + extra, "");
    LLVMValueRef f_offset = LLVMBuildAdd(c->builder, offset,
      LLVMConstInt(c->intptr,
        LLVMOffsetOfElement(c->target_data, structure, i + extra), false), "");

    genserialise_element(c, t->fields[i].type, t->fields[i].embed,
      ctx, field, f_offset);
  }
}

void genserialise_typeid(compile_t* c, reach_type_t* t, LLVMValueRef offset)
{
  // Write the type id instead of the descriptor.
  LLVMValueRef value = LLVMConstInt(c->intptr, t->type_id, false);
  LLVMValueRef loc = LLVMBuildIntToPtr(c->builder, offset,
    LLVMPointerType(c->intptr, 0), "");
  LLVMBuildStore(c->builder, value, loc);
}

void genserialise_element(compile_t* c, reach_type_t* t, bool embed,
  LLVMValueRef ctx, LLVMValueRef ptr, LLVMValueRef offset)
{
  if(embed || (t->underlying == TK_TUPLETYPE))
  {
    // Embedded field or tuple, serialise in place. Don't load from ptr, as
    // it is already a pointer to the object.
    serialise(c, t, ctx, ptr, offset);
  } else if(t->primitive != NULL) {
    // Machine word, write the bits to the buffer.
    LLVMValueRef value = LLVMBuildLoad(c->builder, ptr, "");
    LLVMValueRef loc = LLVMBuildIntToPtr(c->builder, offset,
      LLVMPointerType(t->primitive, 0), "");
    LLVMBuildStore(c->builder, value, loc);
  } else {
    // Lookup the pointer and get the offset, write that.
    LLVMValueRef value = LLVMBuildLoad(c->builder, ptr, "");

    LLVMValueRef args[2];
    args[0] = ctx;
    args[1] = LLVMBuildBitCast(c->builder, value, c->void_ptr, "");
    LLVMValueRef object_offset = gencall_runtime(c, "pony_serialise_offset",
      args, 2, "");

    LLVMValueRef loc = LLVMBuildIntToPtr(c->builder, offset,
      LLVMPointerType(c->intptr, 0), "");
    LLVMBuildStore(c->builder, object_offset, loc);
  }
}

static void make_serialise(compile_t* c, reach_type_t* t)
{
  // Use the trace function as the serialise_trace function.
  t->serialise_trace_fn = t->trace_fn;

  // Generate the serialise function.
  t->serialise_fn = codegen_addfun(c, genname_serialise(t->name),
    c->serialise_type);

  codegen_startfun(c, t->serialise_fn, NULL, NULL);
  LLVMSetFunctionCallConv(t->serialise_fn, LLVMCCallConv);
  LLVMSetLinkage(t->serialise_fn, LLVMExternalLinkage);

  LLVMValueRef ctx = LLVMGetParam(t->serialise_fn, 0);
  LLVMValueRef arg = LLVMGetParam(t->serialise_fn, 1);
  LLVMValueRef addr = LLVMGetParam(t->serialise_fn, 2);
  LLVMValueRef offset = LLVMGetParam(t->serialise_fn, 3);

  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, t->structure_ptr,
    "");
  LLVMValueRef offset_addr = LLVMBuildAdd(c->builder,
    LLVMBuildPtrToInt(c->builder, addr, c->intptr, ""), offset, "");

  serialise(c, t, ctx, object, offset_addr);

  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);
}

static void deserialise(compile_t* c, reach_type_t* t, LLVMValueRef ctx,
  LLVMValueRef object)
{
  // The contents have already been copied.
  int extra = 0;

  switch(t->underlying)
  {
    case TK_CLASS:
    {
      gendeserialise_typeid(c, t, object);
      extra++;
      break;
    }

    case TK_ACTOR:
    {
      // Skip the actor pad.
      gendeserialise_typeid(c, t, object);
      extra += 2;
      break;
    }

    case TK_TUPLETYPE:
    {
      // Get the tuple primitive type.
      if(LLVMTypeOf(object) == t->structure_ptr)
      {
        gendeserialise_typeid(c, t, object);
        object = LLVMBuildStructGEP(c->builder, object, 1, "");
      }
      break;
    }

    default: {}
  }

  for(uint32_t i = 0; i < t->field_count; i++)
  {
    LLVMValueRef field = LLVMBuildStructGEP(c->builder, object, i + extra, "");
    gendeserialise_element(c, t->fields[i].type, t->fields[i].embed,
      ctx, field);
  }
}

void gendeserialise_typeid(compile_t* c, reach_type_t* t, LLVMValueRef object)
{
  // Write the descriptor instead of the type id.
  LLVMValueRef desc_ptr = LLVMBuildStructGEP(c->builder, object, 0, "");
  LLVMBuildStore(c->builder, t->desc, desc_ptr);
}

void gendeserialise_element(compile_t* c, reach_type_t* t, bool embed,
  LLVMValueRef ctx, LLVMValueRef ptr)
{
  if(embed || (t->underlying == TK_TUPLETYPE))
  {
    // Embedded field or tuple, deserialise in place.
    deserialise(c, t, ctx, ptr);
  } else if(t->primitive != NULL) {
    // Machine word, already copied.
  } else {
    // Lookup the pointer and write that.
    LLVMValueRef value = LLVMBuildLoad(c->builder, ptr, "");

    LLVMValueRef args[3];
    args[0] = ctx;
    args[1] = (t->desc != NULL) ?
      LLVMBuildBitCast(c->builder, t->desc, c->descriptor_ptr, "") :
      LLVMConstNull(c->descriptor_ptr);
    args[2] = LLVMBuildPtrToInt(c->builder, value, c->intptr, "");
    LLVMValueRef object = gencall_runtime(c, "pony_deserialise_offset",
      args, 3, "");

    object = LLVMBuildBitCast(c->builder, object, t->use_type, "");
    LLVMBuildStore(c->builder, object, ptr);
  }
}

static void make_deserialise(compile_t* c, reach_type_t* t)
{
  // Generate the deserialise function.
  t->deserialise_fn = codegen_addfun(c, genname_deserialise(t->name),
    c->trace_type);

  codegen_startfun(c, t->deserialise_fn, NULL, NULL);
  LLVMSetFunctionCallConv(t->deserialise_fn, LLVMCCallConv);
  LLVMSetLinkage(t->deserialise_fn, LLVMExternalLinkage);

  LLVMValueRef ctx = LLVMGetParam(t->deserialise_fn, 0);
  LLVMValueRef arg = LLVMGetParam(t->deserialise_fn, 1);

  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, t->structure_ptr,
    "");

  // At this point, the serialised contents have been copied to the allocated
  // object.
  deserialise(c, t, ctx, object);

  LLVMBuildRetVoid(c->builder);
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
        // Don't serialise MaybePointer[A]
        if(name == c->str_Maybe)
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

  if(t->underlying != TK_PRIMITIVE)
    make_deserialise(c, t);

  return true;
}
