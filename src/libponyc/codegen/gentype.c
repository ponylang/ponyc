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

  for(int i = 0; i < count; i++)
  {
    elements[i + extra] = gentype(c, fields[i]);

    if(elements[i + extra] == NULL)
      return NULL;
  }

  LLVMStructSetBody(type, elements, count + extra, false);

  if(!make_trace(c, name, type, fields, count, extra))
    return NULL;

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

<<<<<<< HEAD
static bool make_methods(compile_t* c, ast_t* ast)
{
  assert(ast_id(ast) == TK_NOMINAL);

  ast_t* def = (ast_t*)ast_data(ast);
  ast_t* members = ast_childidx(def, 4);
  ast_t* member = ast_child(members);
  bool actor = ast_id(def) == TK_ACTOR;
  int be_index = 0;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_NEW:
      {
        ast_t* id;
        ast_t* typeparams;
        AST_GET_CHILDREN(member, NULL, &id, &typeparams)

        if(ast_id(typeparams) != TK_NONE)
        {
          ast_error(typeparams,
            "not implemented (codegen for polymorphic constructors)");
          return false;
        }

        LLVMValueRef fun;

        if(actor)
          fun = genfun_newbe(c, ast, ast_name(id), NULL, be_index++);
        else
          fun = genfun_new(c, ast, ast_name(id), NULL);

        if(fun == NULL)
          return false;
        break;
      }

      case TK_BE:
      {
        ast_t* id;
        ast_t* typeparams;
        AST_GET_CHILDREN(member, NULL, &id, &typeparams)

        if(ast_id(typeparams) != TK_NONE)
        {
          ast_error(typeparams,
            "not implemented (codegen for polymorphic behaviours)");
          return false;
        }

        LLVMValueRef fun = genfun_be(c, ast, ast_name(id), NULL, be_index++);

        if(fun == NULL)
          return false;
        break;
      }

      case TK_FUN:
      {
        ast_t* id;
        ast_t* typeparams;
        AST_GET_CHILDREN(member, NULL, &id, &typeparams)

        if(ast_id(typeparams) != TK_NONE)
        {
          ast_error(typeparams,
            "not implemented (codegen for polymorphic functions)");
          return false;
        }

        LLVMValueRef fun = genfun_fun(c, ast, ast_name(id), NULL);

        if(fun == NULL)
          return false;
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  return true;
}

static LLVMValueRef make_function_ptr(compile_t* c, const char* name,
  LLVMTypeRef type)
{
  LLVMValueRef func = LLVMGetNamedFunction(c->module, name);

  if(func == NULL)
    func = LLVMConstNull(type);
  else
    func = LLVMConstBitCast(func, type);

  return func;
}

static void make_descriptor(compile_t* c, ast_t* def, const char* name,
  const char* desc_name, size_t vtable_size, LLVMTypeRef type,
  LLVMTypeRef desc_type, LLVMValueRef g_desc)
{
  // build the actual vtable
  PONY_VL_ARRAY(LLVMValueRef, vtable, vtable_size);

  ast_t* members = ast_childidx(def, 4);
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_BE:
      case TK_FUN:
      {
        ast_t* id = ast_childidx(member, 1);
        const char* funname = ast_name(id);
        const char* fullname = genname_fun(name, funname, NULL);
        int colour = painter_get_colour(c->painter, funname);
        vtable[colour] = make_function_ptr(c, fullname, c->void_ptr);
        assert(vtable[colour] != NULL);
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  // TODO: trait list
  LLVMValueRef args[8];
  args[0] = make_function_ptr(c, genname_trace(name), c->trace_fn);
  args[1] = make_function_ptr(c, genname_serialise(name), c->trace_fn);
  args[2] = make_function_ptr(c, genname_deserialise(name), c->trace_fn);
  args[3] = make_function_ptr(c, genname_dispatch(name), c->dispatch_fn);
  args[4] = make_function_ptr(c, genname_finalise(name), c->trace_fn);
  args[5] = LLVMConstInt(LLVMInt64Type(), LLVMABISizeOfType(c->target, type),
    false);
  args[6] = LLVMConstNull(c->void_ptr);
  args[7] = LLVMConstArray(c->void_ptr, vtable, (unsigned int)vtable_size);;

  LLVMValueRef desc = LLVMConstNamedStruct(desc_type, args, 8);
  LLVMSetInitializer(g_desc, desc);
  LLVMSetGlobalConstant(g_desc, true);
}

=======
>>>>>>> 94cbe5735396edf2d7d0ce121d24c617dfd4887c
static LLVMTypeRef make_object(compile_t* c, ast_t* ast, bool* exists)
{
  const char* name = genname_type(ast);
  LLVMTypeRef type = LLVMGetTypeByName(c->module, name);
  *exists = type != NULL;

  if(type != NULL)
    return LLVMPointerType(type, 0);

  ast_t* def = (ast_t*)ast_data(ast);
  bool actor = ast_id(def) == TK_ACTOR;

  int count;
  ast_t** fields = get_fields(ast, &count);
  type = make_struct(c, name, fields, count, actor);
  free_fields(fields, count);

  if(type == NULL)
    return NULL;

<<<<<<< HEAD
  const char* desc_name = genname_descriptor(name);
  size_t vtable_size = painter_get_vtable_size(c->painter, def);

  LLVMTypeRef desc_type = codegen_desctype(c, name, (unsigned int)vtable_size);
  LLVMValueRef g_desc = LLVMAddGlobal(c->module, desc_type, desc_name);
=======
  gendesc_prep(c, ast, type);
>>>>>>> 94cbe5735396edf2d7d0ce121d24c617dfd4887c

  if(!genfun_methods(c, ast))
    return NULL;

  gendesc_init(c, ast, type, false);
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
<<<<<<< HEAD
  ast_t* def = (ast_t*)ast_data(ast);
=======

  // generate a primitive type if we've encountered one
  LLVMTypeRef type = genprim(c, ast);

  if(type != GEN_NOTYPE)
    return type;

  ast_t* def = ast_data(ast);
>>>>>>> 94cbe5735396edf2d7d0ce121d24c617dfd4887c

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
