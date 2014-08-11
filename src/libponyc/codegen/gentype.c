#include "gentype.h"
#include "genname.h"
#include "gencall.h"
#include "genfun.h"
#include "../pkg/package.h"
#include "../type/cap.h"
#include "../type/reify.h"
#include "../type/subtype.h"
#include <string.h>
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
    ast_t* ast = fields[i];

    switch(ast_id(ast))
    {
      case TK_UNIONTYPE:
      {
        if(!is_bool(ast))
        {
          bool tag = cap_for_type(ast) == TK_TAG;

          if(tag)
          {
            // TODO: are we really a tag? need runtime info
          } else {
            // this union type can never be a tag
            gencall_traceunknown(c, field);
          }
        }
        break;
      }

      case TK_TUPLETYPE:
        gencall_traceknown(c, field, genname_type(ast));
        break;

      case TK_NOMINAL:
      {
        bool tag = cap_for_type(ast) == TK_TAG;

        switch(ast_id((ast_t*)ast_data(ast)))
        {
          case TK_TRAIT:
            if(tag)
              gencall_tracetag(c, field);
            else
              gencall_traceunknown(c, field);
            break;

          case TK_DATA:
            // do nothing
            break;

          case TK_CLASS:
            if(tag)
              gencall_tracetag(c, field);
            else
              gencall_traceknown(c, field, genname_type(ast));
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

      case TK_ISECTTYPE:
      case TK_STRUCTURAL:
      {
        bool tag = cap_for_type(ast) == TK_TAG;

        if(tag)
          gencall_tracetag(c, field);
        else
          gencall_traceunknown(c, field);
        break;
      }

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

  if(make_trace(c, name, type, fields, count, extra) == NULL)
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
  args[7] = LLVMConstArray(c->void_ptr, vtable, vtable_size);;

  LLVMValueRef desc = LLVMConstNamedStruct(desc_type, args, 8);
  LLVMSetInitializer(g_desc, desc);
  LLVMSetGlobalConstant(g_desc, true);
}

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

  const char* desc_name = genname_descriptor(name);
  size_t vtable_size = painter_get_vtable_size(c->painter, def);

  LLVMTypeRef desc_type = codegen_desctype(c, name, vtable_size);
  LLVMValueRef g_desc = LLVMAddGlobal(c->module, desc_type, desc_name);

  if(!make_methods(c, ast))
    return NULL;

  make_descriptor(c, def, name, desc_name, vtable_size, type, desc_type,
    g_desc);

  return LLVMPointerType(type, 0);
}

static LLVMTypeRef gentype_data(compile_t* c, ast_t* ast)
{
  // TODO: create the primitive descriptors
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

  if(!strcmp(name, "I128") || !strcmp(name, "U128") ||
    !strcmp(name, "SIntLiteral") || !strcmp(name, "UIntLiteral")
    )
    return LLVMIntType(128);

  if(!strcmp(name, "F16"))
    return LLVMHalfType();

  if(!strcmp(name, "F32"))
    return LLVMFloatType();

  if(!strcmp(name, "F64") || !strcmp(name, "FloatLiteral"))
    return LLVMDoubleType();

  bool exists;
  LLVMTypeRef type = make_object(c, ast, &exists);

  if(exists || (type == NULL))
    return type;

  // TODO: create singleton instance if not a primitive
  return type;
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
  return type;
}

static LLVMTypeRef gentype_nominal(compile_t* c, ast_t* ast)
{
  assert(ast_id(ast) == TK_NOMINAL);
  ast_t* def = (ast_t*)ast_data(ast);

  switch(ast_id(def))
  {
    case TK_TRAIT:
      // just a raw object pointer
      return c->object_ptr;

    case TK_DATA:
      return gentype_data(c, ast);

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
