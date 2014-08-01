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

static LLVMValueRef make_trace(compile_t* c, const char* name,
  LLVMTypeRef type, ast_t** fields, int count, int extra)
{
  // create a trace function
  const char* trace_name = genname_trace(name);
  LLVMValueRef trace_fn = LLVMAddFunction(c->module, trace_name, c->trace_type);

  LLVMValueRef arg = LLVMGetParam(trace_fn, 0);
  LLVMSetValueName(arg, "arg");

  LLVMBasicBlockRef block = LLVMAppendBasicBlock(trace_fn, "entry");
  LLVMPositionBuilderAtEnd(c->builder, block);
  LLVMTypeRef type_ptr = LLVMPointerType(type, 0);
  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, type_ptr, "object");

  for(int i = 0; i < count; i++)
  {
    LLVMValueRef field = LLVMBuildStructGEP(c->builder, object, i + extra, "");
    gencall_trace(c, field, fields[i]);
  }

  LLVMBuildRetVoid(c->builder);

  if(!codegen_finishfun(c, trace_fn))
    return NULL;

  return trace_fn;
}

static LLVMTypeRef make_struct(compile_t* c, const char* name,
  ast_t** fields, int count, bool actor)
{
  LLVMTypeRef type = LLVMStructCreateNamed(LLVMGetGlobalContext(), name);
  int extra = 1;

  if(actor)
    extra++;

  // create the type descriptor as element 0
  LLVMTypeRef elements[count + extra];
  elements[0] = c->descriptor_ptr;

  if(actor)
    elements[1] = c->actor_pad;

  for(int i = 0; i < count; i++)
  {
    elements[i + extra] = gentype(c, fields[i]);

    if(elements[i + extra] == NULL)
      return NULL;
  }

  LLVMStructSetBody(type, elements, count + extra, false);

  if(make_trace(c, name, type, fields, count, extra) == NULL)
    return NULL;

  return type;
}

static ast_t** get_fields(ast_t* ast, int* count)
{
  assert(ast_id(ast) == TK_NOMINAL);
  ast_t* def = ast_data(ast);

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

static void free_fields(ast_t** fields, int count)
{
  for(int i = 0; i < count; i++)
    ast_free_unattached(fields[i]);

  free(fields);
}

static LLVMTypeRef make_object(compile_t* c, ast_t* ast, bool* exists)
{
  const char* name = genname_type(ast);
  LLVMTypeRef type = LLVMGetTypeByName(c->module, name);
  *exists = type != NULL;

  if(type != NULL)
    return LLVMPointerType(type, 0);

  ast_t* def = ast_data(ast);
  bool actor = ast_id(def) == TK_ACTOR;

  int count;
  ast_t** fields = get_fields(ast, &count);
  type = make_struct(c, name, fields, count, actor);
  free_fields(fields, count);

  if(type == NULL)
    return NULL;

  gendesc_prep(c, ast, type);

  if(!genfun_methods(c, ast))
    return NULL;

  gendesc_init(c, ast, type);
  return LLVMPointerType(type, 0);
}

static LLVMTypeRef gentype_class(compile_t* c, ast_t* ast)
{
  bool exists;
  LLVMTypeRef type = make_object(c, ast, &exists);

  if(exists || (type == NULL))
    return type;

  return type;
}

static LLVMTypeRef gentype_actor(compile_t* c, ast_t* ast)
{
  bool exists;
  LLVMTypeRef type = make_object(c, ast, &exists);

  if(exists || (type == NULL))
    return type;

  // TODO: create a message type function, dispatch function
  // instead of a message type function, could handle tracing ourselves
  // so that send has no prep function
  // would also have to handle receive tracing
  return type;
}

static LLVMTypeRef gentype_nominal(compile_t* c, ast_t* ast)
{
  assert(ast_id(ast) == TK_NOMINAL);

  // generate a primitive type if we've encountered one
  LLVMTypeRef type = genprim(c, ast);

  if(type != NULL)
    return type;

  ast_t* def = ast_data(ast);

  switch(ast_id(def))
  {
    case TK_TRAIT:
      // just a raw object pointer
      return c->object_ptr;

    case TK_DATA:
    case TK_CLASS:
      return gentype_class(c, ast);

    case TK_ACTOR:
      return gentype_actor(c, ast);

    default: {}
  }

  assert(0);
  return NULL;
}

static LLVMTypeRef gentype_tuple(compile_t* c, ast_t* ast)
{
  // an anonymous structure with no functions and no vtable
  const char* name = genname_type(ast);
  LLVMTypeRef type = LLVMGetTypeByName(c->module, name);

  if(type != NULL)
    return LLVMPointerType(type, 0);

  size_t count = ast_childcount(ast);
  ast_t* fields[count];
  ast_t* child = ast_child(ast);
  size_t index = 0;

  while(child != NULL)
  {
    fields[index++] = child;
    child = ast_sibling(child);
  }

  type = make_struct(c, name, fields, count, false);

  if(type == NULL)
    return NULL;

  return LLVMPointerType(type, 0);
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
      LLVMTypeRef type = gentype_tuple(c, ast);
      LLVMPositionBuilderAtEnd(c->builder, insert);
      return type;
    }

    case TK_NOMINAL:
    {
      LLVMBasicBlockRef insert = LLVMGetInsertBlock(c->builder);
      LLVMTypeRef type = gentype_nominal(c, ast);
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
