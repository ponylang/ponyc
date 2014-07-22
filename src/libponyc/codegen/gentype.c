#include "gentype.h"
#include "genname.h"
#include "gencall.h"
#include "../pkg/package.h"
#include "../type/reify.h"
#include "../type/subtype.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

static LLVMTypeRef codegen_struct(compile_t* c, const char* name, ast_t* def,
  ast_t* typeargs)
{
  LLVMTypeRef type = LLVMStructCreateNamed(LLVMGetGlobalContext(), name);

  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* members = ast_childidx(def, 4);
  ast_t* member;

  member = ast_child(members);
  int count = 0;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
        count++;
        break;

      default: {}
    }

    member = ast_sibling(member);
  }

  LLVMTypeRef elements[count];
  ast_t* ftypes[count];
  member = ast_child(members);
  int index = 0;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      {
        ftypes[index] = reify(ast_type(member), typeparams, typeargs);
        LLVMTypeRef ltype = codegen_type(c, ftypes[index]);

        if(ltype == NULL)
        {
          for(int i = 0; i <= index; i++)
            ast_free_unattached(ftypes[i]);

          return NULL;
        }

        elements[index] = ltype;
        index++;
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  LLVMStructSetBody(type, elements, count, false);
  LLVMTypeRef type_ptr = LLVMPointerType(type, 0);

  // create a trace function
  const char* trace_name = codegen_funname(name, "$trace", NULL);
  LLVMValueRef trace_fn = LLVMAddFunction(c->module, trace_name, c->trace_type);

  LLVMValueRef arg = LLVMGetParam(trace_fn, 0);
  LLVMSetValueName(arg, "arg");

  LLVMBasicBlockRef block = LLVMAppendBasicBlock(trace_fn, "entry");
  LLVMPositionBuilderAtEnd(c->builder, block);
  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, type_ptr, "object");

  for(int i = 0; i < count; i++)
  {
    // TODO: pony_trace if it's a tag
    // pony_traceactor if it's an actor
    // pony_traceobject if it's a class
    // nothing if it's a primitive or singleton type
    // what about a trait or type expression?
    LLVMValueRef field = LLVMBuildStructGEP(c->builder, object, i, "");
    LLVMTypeRef field_type = LLVMTypeOf(field);
    (void)field_type;

    ast_free_unattached(ftypes[i]);
  }

  LLVMBuildRetVoid(c->builder);

  if(!codegen_finishfun(c, trace_fn))
    return NULL;

  return type;
}

static LLVMTypeRef codegen_class(compile_t* c, const char* name, ast_t* def,
  ast_t* typeargs)
{
  LLVMTypeRef type = codegen_struct(c, name, def, typeargs);

  if(type == NULL)
    return NULL;

  // TODO: create a type descriptor
  return type;
}

static LLVMTypeRef codegen_actor(compile_t* c, const char* name, ast_t* def,
  ast_t* typeargs)
{
  LLVMTypeRef type = codegen_struct(c, name, def, typeargs);

  if(type == NULL)
    return NULL;

  // TODO: create an actor descriptor, message type function, dispatch function
  return type;
}

static LLVMTypeRef codegen_nominal(compile_t* c, ast_t* ast)
{
  assert(ast_id(ast) == TK_NOMINAL);

  // check for primitive types
  const char* name = ast_name(ast_childidx(ast, 1));

  if(!strcmp(name, "True") || !strcmp(name, "False"))
    return LLVMInt1Type();

  if(!strcmp(name, "I8") || !strcmp(name, "U8"))
    return LLVMInt8Type();

  if(!strcmp(name, "I16") || !strcmp(name, "U16"))
    return LLVMInt16Type();

  if(!strcmp(name, "I32") || !strcmp(name, "U32"))
    return LLVMInt32Type();

  if(!strcmp(name, "I64") || !strcmp(name, "U64"))
    return LLVMInt64Type();

  if(!strcmp(name, "I128") || !strcmp(name, "U128"))
    return LLVMIntType(128);

  if(!strcmp(name, "F16"))
    return LLVMHalfType();

  if(!strcmp(name, "F32"))
    return LLVMFloatType();

  if(!strcmp(name, "F64"))
    return LLVMDoubleType();

  name = codegen_typename(ast);

  if(name == NULL)
    return NULL;

  LLVMTypeRef type = LLVMGetTypeByName(c->module, name);

  if(type != NULL)
    return LLVMPointerType(type, 0);

  ast_t* def = ast_data(ast);
  ast_t* typeargs = ast_childidx(ast, 2);

  switch(ast_id(def))
  {
    case TK_TRAIT:
    {
      ast_error(ast, "not implemented (codegen for trait)");
      return NULL;
    }

    case TK_DATA:
    {
      ast_error(ast, "not implemented (codegen for data)");
      return NULL;
    }

    case TK_CLASS:
    {
      type = codegen_class(c, name, def, typeargs);
      break;
    }

    case TK_ACTOR:
    {
      type = codegen_actor(c, name, def, typeargs);
      break;
    }

    default:
      assert(0);
      return NULL;
  }

  if(type != NULL)
    return LLVMPointerType(type, 0);

  return NULL;
}

static LLVMTypeRef codegen_union(compile_t* c, ast_t* ast)
{
  // special case Bool
  if(is_bool(ast))
    return LLVMInt1Type();

  // return an existing type if it's available
  const char* name = codegen_typename(ast);
  LLVMTypeRef type = LLVMGetTypeByName(c->module, name);

  if(type != NULL)
    return type;

  // union is stored as a byte array big enough to hold every possible type
  // plus a discriminator to decide at runtime which type has been stored
  type = LLVMStructCreateNamed(LLVMGetGlobalContext(), name);

  size_t count = ast_childcount(ast);
  LLVMTypeRef types[count];

  ast_t* child = ast_child(ast);
  size_t index = 0;
  size_t size = 0;

  while(child != NULL)
  {
    types[index] = codegen_type(c, child);

    if(types[index] == NULL)
      return NULL;

    size_t byte_size = LLVMABISizeOfType(c->target, types[index]);

    if(byte_size > size)
      size = byte_size;

    index++;
    child = ast_sibling(child);
  }

  LLVMTypeRef elements[2];
  elements[0] = LLVMInt32Type();
  elements[1] = LLVMArrayType(LLVMInt8Type(), size);
  LLVMStructSetBody(type, elements, 2, false);

  // TODO: trace function? pattern match function?
  return type;
}

static LLVMTypeRef codegen_tuple(compile_t* c, ast_t* ast)
{
  const char* name = codegen_typename(ast);
  LLVMTypeRef type = LLVMGetTypeByName(c->module, name);

  if(type != NULL)
    return type;

  type = LLVMStructCreateNamed(LLVMGetGlobalContext(), name);

  size_t count = ast_childcount(ast);
  LLVMTypeRef elements[count];

  ast_t* child = ast_child(ast);
  size_t index = 0;

  while(child != NULL)
  {
    elements[index] = codegen_type(c, child);

    if(elements[index] == NULL)
      return NULL;

    index++;
    child = ast_sibling(child);
  }

  LLVMStructSetBody(type, elements, count, false);

  // TODO: trace function?
  return type;
}

LLVMTypeRef codegen_type(compile_t* c, ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_UNIONTYPE:
      return codegen_union(c, ast);

    case TK_ISECTTYPE:
    {
      ast_error(ast, "not implemented (codegen for isecttype)");
      return NULL;
    }

    case TK_TUPLETYPE:
      return codegen_tuple(c, ast);

    case TK_NOMINAL:
      return codegen_nominal(c, ast);

    case TK_STRUCTURAL:
    {
      ast_error(ast, "not implemented (codegen for structural)");
      return NULL;
    }

    default: {}
  }

  assert(0);
  return NULL;
}
