#include "gentype.h"
#include "genname.h"
#include "gencall.h"
#include "../pkg/package.h"
#include "../type/reify.h"
#include "../type/subtype.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

static LLVMValueRef gentype_trace(compile_t* c, const char* name,
  LLVMTypeRef type, ast_t** fields, int count)
{
  // create a trace function
  const char* trace_name = genname_fun(name, "$trace", NULL);
  LLVMValueRef trace_fn = LLVMAddFunction(c->module, trace_name, c->trace_type);

  LLVMValueRef arg = LLVMGetParam(trace_fn, 0);
  LLVMSetValueName(arg, "arg");

  LLVMBasicBlockRef block = LLVMAppendBasicBlock(trace_fn, "entry");
  LLVMPositionBuilderAtEnd(c->builder, block);
  LLVMTypeRef type_ptr = LLVMPointerType(type, 0);
  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, type_ptr, "object");

  for(int i = 0; i < count; i++)
  {
    // TODO: underlying instance could be a datatype, an actor, or an object
    // do nothing for a datatype
    // pony_traceactor for an actor
    // pony_trace for a tag object
    // pony_traceobject for a non-tag object
    // what about a type expression? have to determine the capability
    LLVMValueRef field = LLVMBuildStructGEP(c->builder, object, i + 1, "");
    ast_t* ast = fields[i];

    switch(ast_id(ast))
    {
      case TK_UNIONTYPE:
      case TK_ISECTTYPE:
        // TODO:
        break;

      case TK_TUPLETYPE:
        // TODO:
        break;

      case TK_NOMINAL:
      {
        switch(ast_id(ast_data(ast)))
        {
          case TK_TRAIT:
            // TODO: could be an actor or an object
            break;

          case TK_DATA:
            // do nothing
            break;

          case TK_CLASS:
            if(ast_id(ast_childidx(ast, 3)) == TK_TAG)
              gencall_tracetag(c, field);
            else
              gencall_traceclass(c, field, genname_type(ast));
            break;

          case TK_ACTOR:
            gencall_traceactor(c, field);
            break;

          default:
            assert(0);
            return NULL;
        }
        break;
      }

      case TK_STRUCTURAL:
        // TODO: could be anything
        // gentype_calltrace(c, field, ast_id(ast_childidx(ast, 1)));
        break;

      default:
        assert(0);
        return NULL;
    }
  }

  LLVMBuildRetVoid(c->builder);

  if(!codegen_finishfun(c, trace_fn))
    return NULL;

  return trace_fn;
}

static LLVMTypeRef gentype_struct(compile_t* c, const char* name,
  ast_t** fields, int count)
{
  LLVMTypeRef type = LLVMStructCreateNamed(LLVMGetGlobalContext(), name);

  // create the type descriptor as element 0
  LLVMTypeRef elements[count + 1];
  elements[0] = c->descriptor_ptr;

  for(int i = 0; i < count; i++)
  {
    elements[i + 1] = gentype(c, fields[i]);

    if(elements[i + 1] == NULL)
      return NULL;
  }

  LLVMStructSetBody(type, elements, count + 1, false);

  if(gentype_trace(c, name, type, fields, count) == NULL)
    return NULL;

  return type;
}

static ast_t** gentype_getfields(ast_t* def, ast_t* typeargs, int* count)
{
  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* members = ast_childidx(def, 4);
  ast_t* member;

  // count our elements, including the type descriptor as element 0
  member = ast_child(members);
  int n = 0;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
        n++;
        break;

      default: {}
    }

    member = ast_sibling(member);
  }

  ast_t** fields = calloc(n, sizeof(ast_t*));

  member = ast_child(members);
  int index = 0;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      {
        fields[index] = reify(ast_type(member), typeparams, typeargs);
        index++;
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  *count = n;
  return fields;
}

static void gentype_freefields(ast_t** fields, int count)
{
  for(int i = 0; i < count; i++)
    ast_free_unattached(fields[i]);

  free(fields);
}

static LLVMTypeRef gentype_data(compile_t* c, const char* name, ast_t* def,
  ast_t* typeargs)
{
  LLVMTypeRef type = gentype_struct(c, name, NULL, 0);

  if(type == NULL)
    return NULL;

  // TODO: create a type descriptor, singleton instance
  return type;
}

static LLVMTypeRef gentype_class(compile_t* c, const char* name, ast_t* def,
  ast_t* typeargs)
{
  int count;
  ast_t** fields = gentype_getfields(def, typeargs, &count);
  LLVMTypeRef type = gentype_struct(c, name, fields, count);
  gentype_freefields(fields, count);

  if(type == NULL)
    return NULL;

  // TODO: create a type descriptor
  return type;
}

static LLVMTypeRef gentype_actor(compile_t* c, const char* name, ast_t* def,
  ast_t* typeargs)
{
  int count;
  ast_t** fields = gentype_getfields(def, typeargs, &count);
  LLVMTypeRef type = gentype_struct(c, name, fields, count);
  gentype_freefields(fields, count);

  if(type == NULL)
    return NULL;

  // TODO: create an actor descriptor, message type function, dispatch function
  return type;
}

static LLVMTypeRef gentype_nominal(compile_t* c, ast_t* ast)
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

  name = genname_type(ast);

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
      type = gentype_data(c, name, def, typeargs);
      break;
    }

    case TK_CLASS:
    {
      type = gentype_class(c, name, def, typeargs);
      break;
    }

    case TK_ACTOR:
    {
      type = gentype_actor(c, name, def, typeargs);
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

static LLVMTypeRef gentype_union(compile_t* c, ast_t* ast)
{
  // special case Bool
  if(is_bool(ast))
    return LLVMInt1Type();

  // TODO: change this entirely

  // return an existing type if it's available
  const char* name = genname_type(ast);
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
    types[index] = gentype(c, child);

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

static LLVMTypeRef gentype_tuple(compile_t* c, ast_t* ast)
{
  // TODO: wrong? embed a descriptor?
  const char* name = genname_type(ast);
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
    elements[index] = gentype(c, child);

    if(elements[index] == NULL)
      return NULL;

    index++;
    child = ast_sibling(child);
  }

  LLVMStructSetBody(type, elements, count, false);

  // TODO: trace function?
  return type;
}

LLVMTypeRef gentype(compile_t* c, ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_UNIONTYPE:
      return gentype_union(c, ast);

    case TK_ISECTTYPE:
    {
      ast_error(ast, "not implemented (codegen for isecttype)");
      return NULL;
    }

    case TK_TUPLETYPE:
      return gentype_tuple(c, ast);

    case TK_NOMINAL:
      return gentype_nominal(c, ast);

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
