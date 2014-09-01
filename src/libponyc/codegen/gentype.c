#include "gentype.h"
#include "genname.h"
#include "gendesc.h"
#include "genprim.h"
#include "gencall.h"
#include "genfun.h"
#include "../pkg/package.h"
#include "../type/reify.h"
#include "../type/subtype.h"
#include <stdlib.h>
#include <assert.h>

static bool make_trace(compile_t* c, const char* name,
  LLVMTypeRef type, ast_t** fields, int count, bool actor)
{
  // Classes have a descriptor. Actors also have a pad.
  int extra = 1;

  if(actor)
    extra++;

  // create a trace function
  const char* trace_name = genname_trace(name);
  LLVMValueRef trace_fn = LLVMAddFunction(c->module, trace_name, c->trace_type);

  LLVMValueRef arg = LLVMGetParam(trace_fn, 0);
  LLVMSetValueName(arg, "arg");

  LLVMBasicBlockRef block = LLVMAppendBasicBlock(trace_fn, "entry");
  LLVMPositionBuilderAtEnd(c->builder, block);
  LLVMTypeRef type_ptr = LLVMPointerType(type, 0);
  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, type_ptr, "object");

  // if we don't ever trace anything, delete this function
  bool need_trace = false;

  for(int i = 0; i < count; i++)
  {
    LLVMValueRef field = LLVMBuildStructGEP(c->builder, object, i + extra, "");
    need_trace |= gencall_trace(c, field, fields[i]);
  }

  if(!need_trace)
  {
    LLVMDeleteFunction(trace_fn);
    return true;
  }

  LLVMBuildRetVoid(c->builder);
  return codegen_finishfun(c, trace_fn);
}

static LLVMTypeRef make_struct(compile_t* c, const char* name,
  ast_t** fields, int count, bool actor)
{
  LLVMTypeRef type = LLVMStructCreateNamed(LLVMGetGlobalContext(), name);
  int extra = 1;

  if(actor)
    extra++;

  // create the type descriptor as element 0
  PONY_VL_ARRAY(LLVMTypeRef, elements, count + extra);
  elements[0] = c->descriptor_ptr;

  if(actor)
    elements[1] = c->actor_pad;

  // Get a preliminary type for each field and set the struct body. Then
  // generate real types for all the fields. This is needed in case a struct
  // for the type being generated here is required when generating a field.
  for(int i = 0; i < count; i++)
  {
    elements[i + extra] = gentype_prelim(c, fields[i]);

    if(elements[i + extra] == NULL)
      return NULL;
  }

  LLVMStructSetBody(type, elements, count + extra, false);

  for(int i = 0; i < count; i++)
    elements[i + extra] = gentype(c, fields[i]);

  return type;
}

static ast_t** get_fields(ast_t* ast, int* count)
{
  assert(ast_id(ast) == TK_NOMINAL);
  ast_t* def = (ast_t*)ast_data(ast);

  if(ast_id(def) == TK_DATA)
  {
    *count = 0;
    return NULL;
  }

  ast_t* typeargs = ast_childidx(ast, 2);
  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* members = ast_childidx(def, 4);
  ast_t* member;

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

  ast_t** fields = (ast_t**)calloc(n, sizeof(ast_t*));

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

static void free_fields(ast_t** fields, int count)
{
  for(int i = 0; i < count; i++)
    ast_free_unattached(fields[i]);

  free(fields);
}

static LLVMTypeRef make_object(compile_t* c, ast_t* ast)
{
  const char* name = genname_type(ast);
  LLVMTypeRef type = LLVMGetTypeByName(c->module, name);

  if((type != NULL) && !LLVMIsOpaqueStruct(type))
    return LLVMPointerType(type, 0);

  ast_t* def = (ast_t*)ast_data(ast);

  bool actor = ast_id(def) == TK_ACTOR;

  int count;
  ast_t** fields = get_fields(ast, &count);
  type = make_struct(c, name, fields, count, actor);

  if(type == NULL)
  {
    free_fields(fields, count);
    return NULL;
  }

  if(!make_trace(c, name, type, fields, count, actor))
  {
    free_fields(fields, count);
    return NULL;
  }

  free_fields(fields, count);
  gendesc_inst(c, ast);

  if(!genfun_methods(c, ast))
    return NULL;

  // TODO: for actors: create a dispatch function, possibly a finaliser
  gendesc_init(c, ast, false);
  return LLVMPointerType(type, 0);
}

static LLVMTypeRef make_nominal(compile_t* c, ast_t* ast, bool prelim)
{
  assert(ast_id(ast) == TK_NOMINAL);

  // generate a primitive type if we've encountered one
  LLVMTypeRef type = genprim(c, ast, prelim);

  if(type != GEN_NOTYPE)
    return type;

  ast_t* def = (ast_t*)ast_data(ast);

  switch(ast_id(def))
  {
    case TK_TRAIT:
      // just a raw object pointer
      return c->object_ptr;

    case TK_DATA:
    case TK_CLASS:
    case TK_ACTOR:
    {
      if(prelim)
      {
        // Create a preliminary opaque type.
        const char* name = genname_type(ast);
        LLVMTypeRef type = LLVMGetTypeByName(c->module, name);

        if(type == NULL)
          type = LLVMStructCreateNamed(LLVMGetGlobalContext(), name);

        return type;
      }

      return make_object(c, ast);
    }

    default: {}
  }

  assert(0);
  return NULL;
}

static LLVMTypeRef make_tuple(compile_t* c, ast_t* ast)
{
  // an anonymous structure with no functions and no vtable
  const char* name = genname_type(ast);
  LLVMTypeRef type = LLVMGetTypeByName(c->module, name);

  if(type != NULL)
    return LLVMPointerType(type, 0);

  size_t count = ast_childcount(ast);
  PONY_VL_ARRAY(ast_t*, fields, count);
  ast_t* child = ast_child(ast);
  size_t index = 0;

  while(child != NULL)
  {
    fields[index++] = child;
    child = ast_sibling(child);
  }

  type = make_struct(c, name, fields, (int)count, false);

  if(type == NULL)
    return NULL;

  if(!make_trace(c, name, type, fields, (int)count, false))
    return NULL;

  return LLVMPointerType(type, 0);
}

LLVMTypeRef gentype_prelim(compile_t* c, ast_t* ast)
{
  if(ast_id(ast) == TK_NOMINAL)
    return make_nominal(c, ast, true);

  return gentype(c, ast);
}

LLVMTypeRef gentype(compile_t* c, ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_UNIONTYPE:
    {
      // special case Bool
      if(is_bool(ast))
        return LLVMInt1Type();

      // otherwise it's just a raw object pointer
      return c->object_ptr;
    }

    case TK_ISECTTYPE:
      // just a raw object pointer
      return c->object_ptr;

    case TK_TUPLETYPE:
    {
      LLVMBasicBlockRef insert = LLVMGetInsertBlock(c->builder);
      LLVMTypeRef type = make_tuple(c, ast);
      LLVMPositionBuilderAtEnd(c->builder, insert);
      return type;
    }

    case TK_NOMINAL:
    {
      LLVMBasicBlockRef insert = LLVMGetInsertBlock(c->builder);
      LLVMTypeRef type = make_nominal(c, ast, false);
      LLVMPositionBuilderAtEnd(c->builder, insert);
      return type;
    }

    case TK_STRUCTURAL:
      // just a raw object pointer
      return c->object_ptr;

    default: {}
  }

  assert(0);
  return NULL;
}
