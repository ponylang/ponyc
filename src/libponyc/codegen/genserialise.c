#include "codegen.h"
#include "genname.h"

static void serialise(compile_t* c, reach_type_t* t, LLVMValueRef ctx,
  LLVMValueRef object, LLVMValueRef offset)
{
  LLVMTypeRef structure = t->structure;
  int extra = 0;

  switch(t->underlying)
  {
    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
    {
      // Write the type id instead of the descriptor.
      LLVMValueRef type_id = LLVMConstInt(c->intptr, t->type_id, false);
      LLVMValueRef loc = LLVMBuildIntToPtr(c->builder, offset,
        LLVMPointerType(c->intptr, 0), "");
      LLVMBuildStore(c->builder, type_id, loc);
      extra++;
      break;
    }

    case TK_TUPLETYPE:
    {
      // Get the tuple primitive type.
      if(LLVMTypeOf(object) == t->structure_ptr)
        object = LLVMBuildStructGEP(c->builder, object, 1, "");

      structure = t->primitive;
      break;
    }

    default: {}
  }

  // Actors have a pad. Leave the memory untouched.
  if(t->underlying == TK_ACTOR)
    extra++;

  for(uint32_t i = 0; i < t->field_count; i++)
  {
    LLVMValueRef field = LLVMBuildStructGEP(c->builder, object, i + extra, "");
    reach_type_t* t_field = t->fields[i].type;
    LLVMValueRef f_offset = LLVMBuildAdd(c->builder, offset,
      LLVMConstInt(c->intptr,
        LLVMOffsetOfElement(c->target_data, structure, i + extra), false),
      "");

    if(t->fields[i].embed || (t_field->underlying == TK_TUPLETYPE))
    {
      // Embedded field or tuple, serialise in place. Don't load from the
      // StructGEP, as the StructGEP is already a pointer to the object.
      serialise(c, t_field, ctx, field, f_offset);
    } else if(t_field->primitive != NULL) {
      // Machine word, write the bits to the buffer.
      LLVMValueRef value = LLVMBuildLoad(c->builder, field, "");
      LLVMValueRef loc = LLVMBuildIntToPtr(c->builder, f_offset,
        LLVMPointerType(t_field->primitive, 0), "");
      LLVMBuildStore(c->builder, value, loc);
    } else {
      // TODO: pointer
      // lookup the pointer and get the offset, write that
      LLVMValueRef value = LLVMBuildLoad(c->builder, field, "");
      (void)value;
    }
  }
}

static void make_serialise(compile_t* c, reach_type_t* t)
{
  // Generate the serialise function.
  t->serialise_fn = codegen_addfun(c, genname_serialise(t->name),
    c->serialise_type);

  codegen_startfun(c, t->serialise_fn, NULL, NULL);
  LLVMSetFunctionCallConv(t->serialise_fn, LLVMCCallConv);

  LLVMValueRef ctx = LLVMGetParam(t->serialise_fn, 0);
  LLVMValueRef arg = LLVMGetParam(t->serialise_fn, 1);
  LLVMValueRef addr = LLVMGetParam(t->serialise_fn, 2);

  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, t->structure_ptr,
    "");
  LLVMValueRef offset = LLVMBuildPtrToInt(c->builder, addr, c->intptr, "");

  serialise(c, t, ctx, object, offset);

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
        // TODO: could do this by checking for null
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
          // TODO:
          // genprim_array_serialise(c, t);
          return true;
        }

        if(name == c->str_String)
        {
          // TODO:
          // genprim_string_serialise(c, t);
          return true;
        }
      }
      break;
    }

    default:
      return true;
  }

  make_serialise(c, t);
  return true;
}
