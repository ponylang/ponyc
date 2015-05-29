#include "gendesc.h"
#include "genname.h"
#include "gentype.h"
#include "genfun.h"
#include "../type/reify.h"
#include "../ast/stringtab.h"
#include "../../libponyrt/mem/pool.h"
#include <string.h>
#include <assert.h>

#define DESC_ID 0
#define DESC_SIZE 1
#define DESC_TRAIT_COUNT 2
#define DESC_FIELD_COUNT 3
#define DESC_TRACE 4
#define DESC_SERIALISE 5
#define DESC_DESERIALISE 6
#define DESC_DISPATCH 7
#define DESC_FINALISE 8
#define DESC_EVENT_NOTIFY 9
#define DESC_TRAITS 10
#define DESC_FIELDS 11
#define DESC_VTABLE 12

#define DESC_LENGTH 13

static LLVMValueRef make_unbox_function(compile_t* c, gentype_t* g,
  const char* name)
{
  LLVMValueRef fun = LLVMGetNamedFunction(c->module, name);

  if(fun == NULL)
    return LLVMConstNull(c->void_ptr);

  // Create a new unboxing function that forwards to the real function.
  LLVMTypeRef f_type = LLVMGetElementType(LLVMTypeOf(fun));
  int count = LLVMCountParamTypes(f_type);

  // If it takes no arguments, it's a special number constructor. Don't put it
  // in the vtable.
  if(count == 0)
    return LLVMConstNull(c->void_ptr);

  size_t buf_size = count *sizeof(LLVMTypeRef);
  LLVMTypeRef* params = (LLVMTypeRef*)pool_alloc_size(buf_size);
  LLVMGetParamTypes(f_type, params);
  LLVMTypeRef ret_type = LLVMGetReturnType(f_type);

  // It's the same type, but it takes the boxed type instead of the primitive
  // type as the receiver.
  params[0] = g->structure_ptr;

  const char* unbox_name = genname_unbox(name);
  LLVMTypeRef unbox_type = LLVMFunctionType(ret_type, params, count, false);
  LLVMValueRef unbox_fun = codegen_addfun(c, unbox_name, unbox_type);
  codegen_startfun(c, unbox_fun, false);

  // Extract the primitive type from element 1 and call the real function.
  LLVMValueRef this_ptr = LLVMGetParam(unbox_fun, 0);
  LLVMValueRef primitive_ptr = LLVMBuildStructGEP(c->builder, this_ptr, 1, "");
  LLVMValueRef primitive = LLVMBuildLoad(c->builder, primitive_ptr, "");

  LLVMValueRef* args = (LLVMValueRef*)pool_alloc_size(buf_size);
  args[0] = primitive;

  for(int i = 1; i < count; i++)
    args[i] = LLVMGetParam(unbox_fun, i);

  LLVMValueRef result = codegen_call(c, fun, args, count);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  pool_free_size(buf_size, params);
  pool_free_size(buf_size, args);
  return LLVMConstBitCast(unbox_fun, c->void_ptr);
}

static LLVMValueRef make_function_ptr(compile_t* c, const char* name,
  LLVMTypeRef type)
{
  LLVMValueRef fun = LLVMGetNamedFunction(c->module, name);

  if(fun == NULL)
    return LLVMConstNull(type);

  return LLVMConstBitCast(fun, type);
}

static LLVMValueRef make_type_id(compile_t* c, const char* type_name)
{
  // Generate a named constant for the type that is set to a unique integer
  // value for that type.
  const char* name = genname_typeid(type_name);
  LLVMValueRef global = LLVMGetNamedGlobal(c->module, name);

  // Return the constant initialiser, not the global.
  if(global != NULL)
    return LLVMGetInitializer(global);

  global = LLVMAddGlobal(c->module, c->i32, name);
  LLVMSetGlobalConstant(global, true);
  LLVMSetLinkage(global, LLVMInternalLinkage);

  LLVMValueRef id = LLVMConstInt(c->i32, c->next_type_id++, false);
  LLVMSetInitializer(global, id);

  return id;
}

static uint32_t trait_count(compile_t* c, gentype_t* g)
{
  switch(g->underlying)
  {
    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
    {
      reachable_type_t* t = reach_type(c->reachable, g->type_name);
      assert(t != NULL);

      return (uint32_t)reachable_type_cache_size(&t->subtypes);
    }

    default: {}
  }

  return 0;
}

static LLVMValueRef make_trait_count(compile_t* c, gentype_t* g)
{
  return LLVMConstInt(c->i32, trait_count(c, g), false);
}

static LLVMValueRef make_trait_list(compile_t* c, gentype_t* g)
{
  // The list is an array of integers.
  uint32_t count = trait_count(c, g);
  LLVMTypeRef type = LLVMArrayType(c->i32, count);

  // If we have no traits, return a null pointer to a list.
  if(count == 0)
    return LLVMConstNull(LLVMPointerType(type, 0));

  // Create a constant array of trait identifiers.
  size_t buf_size = count *sizeof(LLVMValueRef);
  LLVMValueRef* list = (LLVMValueRef*)pool_alloc_size(buf_size);

  reachable_type_t* t = reach_type(c->reachable, g->type_name);
  assert(t != NULL);

  size_t i = HASHMAP_BEGIN;
  size_t index = 0;
  reachable_type_t* provide;

  while((provide = reachable_type_cache_next(&t->subtypes, &i)) != NULL)
    list[index++] = make_type_id(c, provide->name);

  LLVMValueRef trait_array = LLVMConstArray(c->i32, list, count);

  // Create a global to hold the array.
  const char* name = genname_traitlist(g->type_name);
  LLVMValueRef global = LLVMAddGlobal(c->module, type, name);
  LLVMSetGlobalConstant(global, true);
  LLVMSetLinkage(global, LLVMInternalLinkage);
  LLVMSetInitializer(global, trait_array);

  pool_free_size(buf_size, list);
  return global;
}

static LLVMValueRef make_field_count(compile_t* c, gentype_t* g)
{
  if(g->underlying != TK_TUPLETYPE)
    return LLVMConstInt(c->i32, 0, false);

  return LLVMConstInt(c->i32, g->field_count, false);
}

static LLVMValueRef make_field_list(compile_t* c, gentype_t* g)
{
  // The list is an array of field descriptors.
  int count;

  if(g->underlying == TK_TUPLETYPE)
    count = g->field_count;
  else
    count = 0;

  LLVMTypeRef type = LLVMArrayType(c->field_descriptor, count);

  // If we aren't a tuple, return a null pointer to a list.
  if(count == 0)
    return LLVMConstNull(LLVMPointerType(type, 0));

  // Create a constant array of field descriptors.
  size_t buf_size = count *sizeof(LLVMValueRef);
  LLVMValueRef* list = (LLVMValueRef*)pool_alloc_size(buf_size);

  for(int i = 0; i < count; i++)
  {
    gentype_t fg;

    if(!gentype(c, g->fields[i], &fg))
      return NULL;

    LLVMValueRef fdesc[2];
    fdesc[0] = LLVMConstInt(c->i32,
      LLVMOffsetOfElement(c->target_data, g->primitive, i), false);

    if(fg.desc != NULL)
    {
      // We are a concrete type.
      fdesc[1] = LLVMConstBitCast(fg.desc, c->descriptor_ptr);
    } else {
      // We aren't a concrete type.
      fdesc[1] = LLVMConstNull(c->descriptor_ptr);
    }

    list[i] = LLVMConstStructInContext(c->context, fdesc, 2, false);
  }

  LLVMValueRef field_array = LLVMConstArray(c->field_descriptor, list, count);

  // Create a global to hold the array.
  const char* name = genname_fieldlist(g->type_name);
  LLVMValueRef global = LLVMAddGlobal(c->module, type, name);
  LLVMSetGlobalConstant(global, true);
  LLVMSetLinkage(global, LLVMInternalLinkage);
  LLVMSetInitializer(global, field_array);

  pool_free_size(buf_size, list);
  return global;
}

static LLVMValueRef make_vtable(compile_t* c, gentype_t* g)
{
  uint32_t vtable_size = genfun_vtable_size(c, g);

  if(vtable_size == 0)
    return LLVMConstArray(c->void_ptr, NULL, 0);

  size_t buf_size = vtable_size *sizeof(LLVMValueRef);
  LLVMValueRef* vtable = (LLVMValueRef*)pool_alloc_size(buf_size);
  memset(vtable, 0, buf_size);

  reachable_type_t* t = reach_type(c->reachable, g->type_name);

  size_t i = HASHMAP_BEGIN;
  reachable_method_name_t* n;

  while((n = reachable_method_names_next(&t->methods, &i)) != NULL)
  {
    size_t j = HASHMAP_BEGIN;
    reachable_method_t* m;

    while((m = reachable_methods_next(&n->r_methods, &j)) != NULL)
    {
      const char* fullname = genname_fun(t->name, n->name, m->typeargs);

      switch(ast_id(m->r_fun))
      {
        case TK_NEW:
        case TK_BE:
          if(g->underlying == TK_ACTOR)
            fullname = genname_be(fullname);
          break;

        default: {}
      }

      uint32_t index = m->vtable_index;
      assert(index != (uint32_t)-1);
      assert(vtable[index] == NULL);

      if(g->primitive != NULL)
        vtable[index] = make_unbox_function(c, g, fullname);
      else
        vtable[index] = make_function_ptr(c, fullname, c->void_ptr);
    }
  }

  for(uint32_t i = 0; i < vtable_size; i++)
  {
    if(vtable[i] == NULL)
      vtable[i] = LLVMConstNull(c->void_ptr);
  }

  LLVMValueRef r = LLVMConstArray(c->void_ptr, vtable, vtable_size);
  pool_free_size(buf_size, vtable);
  return r;
}

LLVMTypeRef gendesc_type(compile_t* c, gentype_t* g)
{
  const char* desc_name;
  int traits = 0;
  int fields = 0;
  int vtable_size = 0;

  if(g != NULL)
  {
    desc_name = g->desc_name;
    traits = trait_count(c, g);

    if(g->underlying == TK_TUPLETYPE)
    {
      fields = g->field_count;
    } else {
      vtable_size = genfun_vtable_size(c, g);
    }
  } else {
    desc_name = genname_descriptor(NULL);
  }

  LLVMTypeRef type = LLVMGetTypeByName(c->module, desc_name);

  if(type == NULL)
  {
    type = LLVMStructCreateNamed(c->context, desc_name);
  } else if(!LLVMIsOpaqueStruct(type)) {
    return type;
  }

  LLVMTypeRef params[DESC_LENGTH];

  params[DESC_ID] = c->i32;
  params[DESC_SIZE] = c->i32;
  params[DESC_TRAIT_COUNT] = c->i32;
  params[DESC_FIELD_COUNT] = c->i32;
  params[DESC_TRACE] = c->trace_fn;
  params[DESC_SERIALISE] = c->trace_fn;
  params[DESC_DESERIALISE] = c->trace_fn;
  params[DESC_DISPATCH] = c->dispatch_fn;
  params[DESC_FINALISE] = c->final_fn;
  params[DESC_EVENT_NOTIFY] = c->i32;
  params[DESC_TRAITS] = LLVMPointerType(LLVMArrayType(c->i32, traits), 0);
  params[DESC_FIELDS] = LLVMPointerType(
    LLVMArrayType(c->field_descriptor, fields), 0);
  params[DESC_VTABLE] = LLVMArrayType(c->void_ptr, vtable_size);

  LLVMStructSetBody(type, params, DESC_LENGTH, false);
  return type;
}

void gendesc_init(compile_t* c, gentype_t* g)
{
  // Initialise the global descriptor.
  uint32_t size = (uint32_t)LLVMABISizeOfType(c->target_data, g->structure);

  // Generate a separate type ID for every type.
  LLVMValueRef args[DESC_LENGTH];

  args[DESC_ID] = make_type_id(c, g->type_name);
  args[DESC_SIZE] = LLVMConstInt(c->i32, size, false);
  args[DESC_TRAIT_COUNT] = make_trait_count(c, g);
  args[DESC_FIELD_COUNT] = make_field_count(c, g);
  args[DESC_TRACE] = make_function_ptr(c, genname_trace(g->type_name),
    c->trace_fn);
  args[DESC_SERIALISE] = make_function_ptr(c, genname_serialise(g->type_name),
    c->trace_fn);
  args[DESC_DESERIALISE] = make_function_ptr(c,
    genname_deserialise(g->type_name), c->trace_fn);
  args[DESC_DISPATCH] = make_function_ptr(c, genname_dispatch(g->type_name),
    c->dispatch_fn);
  args[DESC_FINALISE] = make_function_ptr(c, genname_finalise(g->type_name),
    c->final_fn);
  args[DESC_EVENT_NOTIFY] = LLVMConstInt(c->i32,
    genfun_vtable_index(c, g, stringtab("_event_notify"), NULL), false);
  args[DESC_TRAITS] = make_trait_list(c, g);
  args[DESC_FIELDS] = make_field_list(c, g);
  args[DESC_VTABLE] = make_vtable(c, g);

  LLVMValueRef desc = LLVMConstNamedStruct(g->desc_type, args, DESC_LENGTH);
  LLVMSetInitializer(g->desc, desc);
  LLVMSetGlobalConstant(g->desc, true);
}

static LLVMValueRef desc_field(compile_t* c, LLVMValueRef desc, int index)
{
  LLVMValueRef ptr = LLVMBuildStructGEP(c->builder, desc, index, "");
  return LLVMBuildLoad(c->builder, ptr, "");
}

LLVMValueRef gendesc_fetch(compile_t* c, LLVMValueRef object)
{
  LLVMValueRef ptr = LLVMBuildStructGEP(c->builder, object, 0, "");
  return LLVMBuildLoad(c->builder, ptr, "");
}

LLVMValueRef gendesc_trace(compile_t* c, LLVMValueRef object)
{
  return desc_field(c, gendesc_fetch(c, object), DESC_TRACE);
}

LLVMValueRef gendesc_dispatch(compile_t* c, LLVMValueRef object)
{
  return desc_field(c, gendesc_fetch(c, object), DESC_DISPATCH);
}

LLVMValueRef gendesc_vtable(compile_t* c, LLVMValueRef object, size_t colour)
{
  LLVMValueRef desc = gendesc_fetch(c, object);
  LLVMValueRef vtable = LLVMBuildStructGEP(c->builder, desc, DESC_VTABLE, "");

  LLVMValueRef gep[2];
  gep[0] = LLVMConstInt(c->i32, 0, false);
  gep[1] = LLVMConstInt(c->i32, colour, false);

  LLVMValueRef func_ptr = LLVMBuildGEP(c->builder, vtable, gep, 2, "");
  return LLVMBuildLoad(c->builder, func_ptr, "");
}

LLVMValueRef gendesc_fieldcount(compile_t* c, LLVMValueRef desc)
{
  return desc_field(c, desc, DESC_FIELD_COUNT);
}

LLVMValueRef gendesc_fielddesc(compile_t* c, LLVMValueRef desc, size_t index)
{
  LLVMValueRef fields = desc_field(c, desc, DESC_FIELDS);

  LLVMValueRef gep[2];
  gep[0] = LLVMConstInt(c->i32, 0, false);
  gep[1] = LLVMConstInt(c->i32, index, false);

  LLVMValueRef field_desc = LLVMBuildGEP(c->builder, fields, gep, 2, "");
  return LLVMBuildLoad(c->builder, field_desc, "");
}

LLVMValueRef gendesc_istrait(compile_t* c, LLVMValueRef desc, ast_t* type)
{
  // Get the trait identifier.
  LLVMValueRef trait_id = gendesc_typeid(c, type);

  // Read the count and the trait list from the descriptor.
  LLVMValueRef count = desc_field(c, desc, DESC_TRAIT_COUNT);
  LLVMValueRef list = desc_field(c, desc, DESC_TRAITS);

  LLVMBasicBlockRef entry_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef cond_block = codegen_block(c, "cond");
  LLVMBasicBlockRef body_block = codegen_block(c, "body");
  LLVMBasicBlockRef post_block = codegen_block(c, "post");
  LLVMBuildBr(c->builder, cond_block);

  // While the index is less than the count, check an ID.
  LLVMPositionBuilderAtEnd(c->builder, cond_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->i32, "");
  LLVMValueRef zero = LLVMConstInt(c->i32, 0, false);
  LLVMAddIncoming(phi, &zero, &entry_block, 1);

  LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntULT, phi, count, "");
  LLVMBuildCondBr(c->builder, test, body_block, post_block);

  // The phi node is the index. Get ID and compare it.
  LLVMPositionBuilderAtEnd(c->builder, body_block);

  LLVMValueRef gep[2];
  gep[0] = LLVMConstInt(c->i32, 0, false);
  gep[1] = phi;

  LLVMValueRef id_ptr = LLVMBuildGEP(c->builder, list, gep, 2, "");
  LLVMValueRef id = LLVMBuildLoad(c->builder, id_ptr, "");
  LLVMValueRef test_id = LLVMBuildICmp(c->builder, LLVMIntEQ, id, trait_id,
    "");

  // Add one to the phi node.
  LLVMValueRef one = LLVMConstInt(c->i32, 1, false);
  LLVMValueRef inc = LLVMBuildAdd(c->builder, phi, one, "");
  LLVMAddIncoming(phi, &inc, &body_block, 1);

  // Either to the post block or back to the condition.
  LLVMBuildCondBr(c->builder, test_id, post_block, cond_block);

  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef result = LLVMBuildPhi(c->builder, c->i1, "");
  LLVMAddIncoming(result, &test, &cond_block, 1);
  LLVMAddIncoming(result, &test_id, &body_block, 1);

  return result;
}

LLVMValueRef gendesc_typeid(compile_t* c, ast_t* type)
{
  return make_type_id(c, genname_type(type));
}
