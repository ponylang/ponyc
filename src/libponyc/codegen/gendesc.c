#include "gendesc.h"
#include "genexpr.h"
#include "genfun.h"
#include "genname.h"
#include "genopt.h"
#include "gentype.h"
#include "../type/reify.h"
#include "../ast/stringtab.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <string.h>

#define DESC_ID 0
#define DESC_SIZE 1
#define DESC_FIELD_COUNT 2
#define DESC_FIELD_OFFSET 3
#define DESC_INSTANCE 4
#define DESC_TRACE 5
#define DESC_SERIALISE_TRACE 6
#define DESC_SERIALISE 7
#define DESC_DESERIALISE 8
#define DESC_CUSTOM_SERIALISE_SPACE 9
#define DESC_CUSTOM_DESERIALISE 10
#define DESC_DISPATCH 11
#define DESC_FINALISE 12
#define DESC_EVENT_NOTIFY 13
#define DESC_TRAITS 14
#define DESC_FIELDS 15
#define DESC_VTABLE 16

#define DESC_LENGTH 17

static LLVMValueRef make_unbox_function(compile_t* c, reach_type_t* t,
  reach_method_t* m)
{
  // Create a new unboxing function that forwards to the real function.
  compile_method_t* c_m = (compile_method_t*)m->c_method;
  LLVMTypeRef f_type = LLVMGetElementType(LLVMTypeOf(c_m->func));
  int count = LLVMCountParamTypes(f_type);

  // Leave space for a receiver if it's a constructor vtable entry.
  size_t buf_size = (count + 1) * sizeof(LLVMTypeRef);
  LLVMTypeRef* params = (LLVMTypeRef*)ponyint_pool_alloc_size(buf_size);
  LLVMGetParamTypes(f_type, params);
  LLVMTypeRef ret_type = LLVMGetReturnType(f_type);

  const char* unbox_name = genname_unbox(m->full_name);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  if(ast_id(m->fun->ast) != TK_NEW)
  {
    // It's the same type, but it takes the boxed type instead of the primitive
    // type as the receiver.
    params[0] = c_t->structure_ptr;
  } else {
    // For a constructor, the unbox_fun has a receiver, even though the real
    // method does not.
    memmove(&params[1], &params[0], count * sizeof(LLVMTypeRef*));
    params[0] = c_t->structure_ptr;
    count++;
  }

  LLVMTypeRef unbox_type = LLVMFunctionType(ret_type, params, count, false);
  LLVMValueRef unbox_fun = codegen_addfun(c, unbox_name, unbox_type, true);
  codegen_startfun(c, unbox_fun, NULL, NULL, NULL, false);

  // Extract the primitive type from element 1 and call the real function.
  LLVMValueRef this_ptr = LLVMGetParam(unbox_fun, 0);
  LLVMValueRef primitive_ptr = LLVMBuildStructGEP(c->builder, this_ptr, 1, "");
  LLVMValueRef primitive = LLVMBuildLoad(c->builder, primitive_ptr, "");

  primitive = gen_assign_cast(c, c_t->use_type, primitive, t->ast_cap);

  LLVMValueRef* args = (LLVMValueRef*)ponyint_pool_alloc_size(buf_size);

  if(ast_id(m->fun->ast) != TK_NEW)
  {
    // If it's not a constructor, pass the extracted primitive as the receiver.
    args[0] = primitive;

    for(int i = 1; i < count; i++)
      args[i] = LLVMGetParam(unbox_fun, i);
  } else {
    count--;

    for(int i = 0; i < count; i++)
      args[i] = LLVMGetParam(unbox_fun, i + 1);
  }

  LLVMValueRef result = codegen_call(c, c_m->func, args, count, m->cap != TK_AT);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  ponyint_pool_free_size(buf_size, params);
  ponyint_pool_free_size(buf_size, args);
  return LLVMConstBitCast(unbox_fun, c->void_ptr);
}

static LLVMValueRef make_desc_ptr(LLVMValueRef func, LLVMTypeRef type)
{
  if(func == NULL)
    return LLVMConstNull(type);

  return LLVMConstBitCast(func, type);
}

static LLVMValueRef* trait_bitmap32(compile_t* c, reach_type_t* t)
{
  size_t bm_size = c->trait_bitmap_size * sizeof(uint32_t);

  uint32_t* bm = (uint32_t*)ponyint_pool_alloc_size(bm_size);
  memset(bm, 0, bm_size);

  size_t i = HASHMAP_BEGIN;
  reach_type_t* provide;

  while((provide = reach_type_cache_next(&t->subtypes, &i)) != NULL)
  {
    pony_assert(provide->type_id != (uint32_t)-1);
    uint32_t index = provide->type_id >> 5;
    pony_assert(index < c->trait_bitmap_size);
    uint32_t bit = provide->type_id & 31;
    bm[index] |= 1 << bit;
  }

  LLVMValueRef* bitmap = (LLVMValueRef*)ponyint_pool_alloc_size(
    c->trait_bitmap_size * sizeof(LLVMValueRef));

  for(i = 0; i < c->trait_bitmap_size; i++)
    bitmap[i] = LLVMConstInt(c->intptr, bm[i], false);

  ponyint_pool_free_size(bm_size, bm);

  return bitmap;
}

static LLVMValueRef* trait_bitmap64(compile_t* c, reach_type_t* t)
{
  size_t bm_size = c->trait_bitmap_size * sizeof(uint64_t);

  uint64_t* bm = (uint64_t*)ponyint_pool_alloc_size(bm_size);
  memset(bm, 0, bm_size);

  size_t i = HASHMAP_BEGIN;
  reach_type_t* provide;

  while((provide = reach_type_cache_next(&t->subtypes, &i)) != NULL)
  {
    pony_assert(provide->type_id != (uint32_t)-1);
    uint64_t index = provide->type_id >> 6;
    pony_assert(index < c->trait_bitmap_size);
    uint64_t bit = provide->type_id & 63;
    bm[index] |= (uint64_t)1 << bit;
  }

  LLVMValueRef* bitmap = (LLVMValueRef*)ponyint_pool_alloc_size(
    c->trait_bitmap_size * sizeof(LLVMValueRef));

  for(i = 0; i < c->trait_bitmap_size; i++)
    bitmap[i] = LLVMConstInt(c->intptr, bm[i], false);

  ponyint_pool_free_size(bm_size, bm);

  return bitmap;
}

static LLVMValueRef make_trait_bitmap(compile_t* c, reach_type_t* t)
{
  LLVMTypeRef map_type = LLVMArrayType(c->intptr, c->trait_bitmap_size);

  if(t->bare_method != NULL)
    return LLVMConstNull(LLVMPointerType(map_type, 0));

  LLVMValueRef* bitmap;
  if(target_is_ilp32(c->opt->triple))
    bitmap = trait_bitmap32(c, t);
  else
    bitmap = trait_bitmap64(c, t);

  LLVMValueRef bitmap_array = LLVMConstArray(c->intptr, bitmap,
    c->trait_bitmap_size);

  ponyint_pool_free_size(c->trait_bitmap_size * sizeof(LLVMValueRef), bitmap);

  // Create a global to hold the array.
  const char* name = genname_traitmap(t->name);
  LLVMValueRef global = LLVMAddGlobal(c->module, map_type, name);
  LLVMSetGlobalConstant(global, true);
  LLVMSetLinkage(global, LLVMPrivateLinkage);
  LLVMSetInitializer(global, bitmap_array);
  LLVMSetUnnamedAddr(global, true);

  return global;
}

static LLVMValueRef make_field_count(compile_t* c, reach_type_t* t)
{
  if(t->underlying != TK_TUPLETYPE)
    return LLVMConstInt(c->i32, 0, false);

  return LLVMConstInt(c->i32, t->field_count, false);
}

static LLVMValueRef make_field_offset(compile_t* c, reach_type_t* t)
{
  if(t->field_count == 0)
    return LLVMConstInt(c->i32, 0, false);

  int index = 0;

  if(t->underlying != TK_STRUCT)
    index++;

  if(t->underlying == TK_ACTOR)
    index++;

  compile_type_t* c_t = (compile_type_t*)t->c_type;
  return LLVMConstInt(c->i32,
    LLVMOffsetOfElement(c->target_data, c_t->structure, index), false);
}

static LLVMValueRef make_field_list(compile_t* c, reach_type_t* t)
{
  // The list is an array of field descriptors.
  uint32_t count;

  if(t->underlying == TK_TUPLETYPE)
    count = t->field_count;
  else
    count = 0;

  LLVMTypeRef field_type = LLVMArrayType(c->field_descriptor, count);

  // If we aren't a tuple, return a null pointer to a list.
  if(count == 0)
    return LLVMConstNull(LLVMPointerType(field_type, 0));

  // Create a constant array of field descriptors.
  size_t buf_size = count * sizeof(LLVMValueRef);
  LLVMValueRef* list = (LLVMValueRef*)ponyint_pool_alloc_size(buf_size);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  for(uint32_t i = 0; i < count; i++)
  {
    LLVMValueRef fdesc[2];
    fdesc[0] = LLVMConstInt(c->i32, LLVMOffsetOfElement(c->target_data,
      c_t->primitive, i), false);
    compile_type_t* f_c_t = (compile_type_t*)t->fields[i].type->c_type;

    if(f_c_t->desc != NULL)
    {
      // We are a concrete type.
      fdesc[1] = LLVMConstBitCast(f_c_t->desc, c->descriptor_ptr);
    } else {
      // We aren't a concrete type.
      fdesc[1] = LLVMConstNull(c->descriptor_ptr);
    }

    list[i] = LLVMConstStructInContext(c->context, fdesc, 2, false);
  }

  LLVMValueRef field_array = LLVMConstArray(c->field_descriptor, list, count);

  // Create a global to hold the array.
  const char* name = genname_fieldlist(t->name);
  LLVMValueRef global = LLVMAddGlobal(c->module, field_type, name);
  LLVMSetGlobalConstant(global, true);
  LLVMSetLinkage(global, LLVMPrivateLinkage);
  LLVMSetInitializer(global, field_array);
  LLVMSetUnnamedAddr(global, true);

  ponyint_pool_free_size(buf_size, list);
  return global;
}

static LLVMValueRef make_vtable(compile_t* c, reach_type_t* t)
{
  if(t->vtable_size == 0)
    return LLVMConstArray(c->void_ptr, NULL, 0);

  size_t buf_size = t->vtable_size * sizeof(LLVMValueRef);
  LLVMValueRef* vtable = (LLVMValueRef*)ponyint_pool_alloc_size(buf_size);
  memset(vtable, 0, buf_size);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  size_t i = HASHMAP_BEGIN;
  reach_method_name_t* n;

  while((n = reach_method_names_next(&t->methods, &i)) != NULL)
  {
    size_t j = HASHMAP_BEGIN;
    reach_method_t* m;

    while((m = reach_mangled_next(&n->r_mangled, &j)) != NULL)
    {
      if(!m->needs_vtable_index)
        continue;

      uint32_t index = m->vtable_index;
      pony_assert(index != (uint32_t)-1);
      pony_assert(vtable[index] == NULL);
      compile_method_t* c_m = (compile_method_t*)m->c_method;

      if((c_t->primitive != NULL) && !m->internal)
        vtable[index] = make_unbox_function(c, t, m);
      else
        vtable[index] = make_desc_ptr(c_m->func, c->void_ptr);
    }
  }

  for(uint32_t i = 0; i < t->vtable_size; i++)
  {
    if(vtable[i] == NULL)
      vtable[i] = LLVMConstNull(c->void_ptr);
  }

  LLVMValueRef r = LLVMConstArray(c->void_ptr, vtable, t->vtable_size);
  ponyint_pool_free_size(buf_size, vtable);
  return r;
}

void gendesc_basetype(compile_t* c, LLVMTypeRef desc_type)
{
  LLVMTypeRef params[DESC_LENGTH];

  params[DESC_ID] = c->i32;
  params[DESC_SIZE] = c->i32;
  params[DESC_FIELD_COUNT] = c->i32;
  params[DESC_FIELD_OFFSET] = c->i32;
  params[DESC_INSTANCE] = c->object_ptr;
  params[DESC_TRACE] = c->trace_fn;
  params[DESC_SERIALISE_TRACE] = c->trace_fn;
  params[DESC_SERIALISE] = c->serialise_fn;
  params[DESC_DESERIALISE] = c->trace_fn;
  params[DESC_CUSTOM_SERIALISE_SPACE] = c->custom_serialise_space_fn;
  params[DESC_CUSTOM_DESERIALISE] = c->custom_deserialise_fn;
  params[DESC_DISPATCH] = c->dispatch_fn;
  params[DESC_FINALISE] = c->final_fn;
  params[DESC_EVENT_NOTIFY] = c->i32;
  params[DESC_TRAITS] = LLVMPointerType(
    LLVMArrayType(c->intptr, 0), 0);
  params[DESC_FIELDS] = LLVMPointerType(
    LLVMArrayType(c->field_descriptor, 0), 0);
  params[DESC_VTABLE] = LLVMArrayType(c->void_ptr, 0);

  LLVMStructSetBody(desc_type, params, DESC_LENGTH, false);
}

void gendesc_type(compile_t* c, reach_type_t* t)
{
  switch(t->underlying)
  {
    case TK_TUPLETYPE:
    case TK_PRIMITIVE:
    case TK_STRUCT:
    case TK_CLASS:
    case TK_ACTOR:
      break;

    default:
      return;
  }

  const char* desc_name = genname_descriptor(t->name);
  uint32_t fields = 0;
  uint32_t vtable_size = t->vtable_size;

  if(t->underlying == TK_TUPLETYPE)
    fields = t->field_count;
  else
    vtable_size = t->vtable_size;

  compile_type_t* c_t = (compile_type_t*)t->c_type;
  c_t->desc_type = LLVMStructCreateNamed(c->context, desc_name);
  LLVMTypeRef params[DESC_LENGTH];

  params[DESC_ID] = c->i32;
  params[DESC_SIZE] = c->i32;
  params[DESC_FIELD_COUNT] = c->i32;
  params[DESC_FIELD_OFFSET] = c->i32;
  params[DESC_INSTANCE] = c->object_ptr;
  params[DESC_TRACE] = c->trace_fn;
  params[DESC_SERIALISE_TRACE] = c->trace_fn;
  params[DESC_SERIALISE] = c->serialise_fn;
  params[DESC_DESERIALISE] = c->trace_fn;
  params[DESC_CUSTOM_SERIALISE_SPACE] = c->custom_serialise_space_fn;
  params[DESC_CUSTOM_DESERIALISE] = c->custom_deserialise_fn;
  params[DESC_DISPATCH] = c->dispatch_fn;
  params[DESC_FINALISE] = c->final_fn;
  params[DESC_EVENT_NOTIFY] = c->i32;
  params[DESC_TRAITS] = LLVMPointerType(
    LLVMArrayType(c->intptr, c->trait_bitmap_size), 0);
  params[DESC_FIELDS] = LLVMPointerType(
    LLVMArrayType(c->field_descriptor, fields), 0);
  params[DESC_VTABLE] = LLVMArrayType(c->void_ptr, vtable_size);

  LLVMStructSetBody(c_t->desc_type, params, DESC_LENGTH, false);

  c_t->desc = LLVMAddGlobal(c->module, c_t->desc_type, desc_name);
  LLVMSetGlobalConstant(c_t->desc, true);
  LLVMSetLinkage(c_t->desc, LLVMPrivateLinkage);
}

void gendesc_init(compile_t* c, reach_type_t* t)
{
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  if(c_t->desc_type == NULL)
    return;

  // Initialise the global descriptor.
  uint32_t event_notify_index = reach_vtable_index(t, c->str__event_notify);

  LLVMValueRef args[DESC_LENGTH];
  args[DESC_ID] = LLVMConstInt(c->i32, t->type_id, false);
  args[DESC_SIZE] = LLVMConstInt(c->i32, c_t->abi_size, false);
  args[DESC_FIELD_COUNT] = make_field_count(c, t);
  args[DESC_FIELD_OFFSET] = make_field_offset(c, t);
  args[DESC_INSTANCE] = make_desc_ptr(c_t->instance, c->object_ptr);
  args[DESC_TRACE] = make_desc_ptr(c_t->trace_fn, c->trace_fn);
  args[DESC_SERIALISE_TRACE] = make_desc_ptr(c_t->serialise_trace_fn,
    c->trace_fn);
  args[DESC_SERIALISE] = make_desc_ptr(c_t->serialise_fn, c->serialise_fn);
  args[DESC_DESERIALISE] = make_desc_ptr(c_t->deserialise_fn, c->trace_fn);
  args[DESC_CUSTOM_SERIALISE_SPACE] =
    make_desc_ptr(c_t->custom_serialise_space_fn, c->custom_serialise_space_fn);
  args[DESC_CUSTOM_DESERIALISE] =
    make_desc_ptr(c_t->custom_deserialise_fn, c->custom_deserialise_fn);
  args[DESC_DISPATCH] = make_desc_ptr(c_t->dispatch_fn, c->dispatch_fn);
  args[DESC_FINALISE] = make_desc_ptr(c_t->final_fn, c->final_fn);
  args[DESC_EVENT_NOTIFY] = LLVMConstInt(c->i32, event_notify_index, false);
  args[DESC_TRAITS] = make_trait_bitmap(c, t);
  args[DESC_FIELDS] = make_field_list(c, t);
  args[DESC_VTABLE] = make_vtable(c, t);

  LLVMValueRef desc = LLVMConstNamedStruct(c_t->desc_type, args, DESC_LENGTH);
  LLVMSetInitializer(c_t->desc, desc);
  LLVMSetGlobalConstant(c_t->desc, true);
}

void gendesc_table(compile_t* c)
{
  uint32_t len = reach_max_type_id(c->reach);

  size_t size = len * sizeof(LLVMValueRef);
  LLVMValueRef* args = (LLVMValueRef*)ponyint_pool_alloc_size(size);

  LLVMValueRef null = LLVMConstNull(c->descriptor_ptr);
  for(size_t i = 0; i < len; i++)
    args[i] = null;

  reach_type_t* t;
  size_t i = HASHMAP_BEGIN;

  while((t = reach_types_next(&c->reach->types, &i)) != NULL)
  {
    if(t->is_trait || (t->underlying == TK_STRUCT))
      continue;

    compile_type_t* c_t = (compile_type_t*)t->c_type;
    LLVMValueRef desc;

    if(c_t->desc != NULL)
      desc = LLVMBuildBitCast(c->builder, c_t->desc, c->descriptor_ptr, "");
    else
      desc = LLVMConstNull(c->descriptor_ptr);

    args[t->type_id] = desc;
  }

  LLVMTypeRef type = LLVMArrayType(c->descriptor_ptr, len);
  LLVMValueRef table = LLVMAddGlobal(c->module, type, "__DescTable");
  LLVMValueRef value = LLVMConstArray(c->descriptor_ptr, args, len);
  LLVMSetInitializer(table, value);
  LLVMSetGlobalConstant(table, true);
  LLVMSetLinkage(table, LLVMPrivateLinkage);
  c->desc_table = table;

  ponyint_pool_free_size(size, args);
}

static LLVMValueRef desc_field(compile_t* c, LLVMValueRef desc, int index)
{
  LLVMValueRef ptr = LLVMBuildStructGEP(c->builder, desc, index, "");
  LLVMValueRef field = LLVMBuildLoad(c->builder, ptr, "");
  return field;
}

LLVMValueRef gendesc_fetch(compile_t* c, LLVMValueRef object)
{
  LLVMValueRef ptr = LLVMBuildStructGEP(c->builder, object, 0, "");
  LLVMValueRef desc = LLVMBuildLoad(c->builder, ptr, "");
  return desc;
}

LLVMValueRef gendesc_typeid(compile_t* c, LLVMValueRef desc)
{
  return desc_field(c, desc, DESC_ID);
}

LLVMValueRef gendesc_instance(compile_t* c, LLVMValueRef desc)
{
  return desc_field(c, desc, DESC_INSTANCE);
}

LLVMValueRef gendesc_trace(compile_t* c, LLVMValueRef desc)
{
  return desc_field(c, desc, DESC_TRACE);
}

LLVMValueRef gendesc_dispatch(compile_t* c, LLVMValueRef desc)
{
  return desc_field(c, desc, DESC_DISPATCH);
}

LLVMValueRef gendesc_vtable(compile_t* c, LLVMValueRef desc, size_t colour)
{
  pony_assert(colour != (size_t)-1);

  LLVMValueRef vtable = LLVMBuildStructGEP(c->builder, desc, DESC_VTABLE, "");

  LLVMValueRef gep[2];
  gep[0] = LLVMConstInt(c->i32, 0, false);
  gep[1] = LLVMConstInt(c->i32, colour, false);

  LLVMValueRef func_ptr = LLVMBuildInBoundsGEP(c->builder, vtable, gep, 2, "");
  LLVMValueRef fun = LLVMBuildLoad(c->builder, func_ptr, "");
  return fun;
}

LLVMValueRef gendesc_ptr_to_fields(compile_t* c, LLVMValueRef object,
  LLVMValueRef desc)
{
  // Skip the descriptor.
  LLVMValueRef offset = desc_field(c, desc, DESC_FIELD_OFFSET);
  offset = LLVMBuildZExt(c->builder, offset, c->intptr, "");

  LLVMValueRef base = LLVMBuildBitCast(c->builder, object, c->void_ptr, "");
  return LLVMBuildInBoundsGEP(c->builder, base, &offset, 1, "");
}

LLVMValueRef gendesc_fieldcount(compile_t* c, LLVMValueRef desc)
{
  return desc_field(c, desc, DESC_FIELD_COUNT);
}

LLVMValueRef gendesc_fieldinfo(compile_t* c, LLVMValueRef desc, size_t index)
{
  LLVMValueRef fields = desc_field(c, desc, DESC_FIELDS);

  LLVMValueRef gep[2];
  gep[0] = LLVMConstInt(c->i32, 0, false);
  gep[1] = LLVMConstInt(c->i32, index, false);

  LLVMValueRef field_desc = LLVMBuildInBoundsGEP(c->builder, fields, gep, 2,
    "");
  LLVMValueRef field_info = LLVMBuildLoad(c->builder, field_desc, "");
  return field_info;
}

LLVMValueRef gendesc_fieldptr(compile_t* c, LLVMValueRef ptr,
  LLVMValueRef field_info)
{
  LLVMValueRef offset = LLVMBuildExtractValue(c->builder, field_info, 0, "");
  offset = LLVMBuildZExt(c->builder, offset, c->intptr, "");
  return LLVMBuildInBoundsGEP(c->builder, ptr, &offset, 1, "");
}

LLVMValueRef gendesc_fieldload(compile_t* c, LLVMValueRef ptr,
  LLVMValueRef field_info)
{
  LLVMValueRef field_ptr = gendesc_fieldptr(c, ptr, field_info);
  LLVMValueRef object_ptr = LLVMBuildBitCast(c->builder, field_ptr,
    LLVMPointerType(c->object_ptr, 0), "");
  // gendesc_fieldload is called from trace functions, no need for TBAA metadata
  // here.
  return LLVMBuildLoad(c->builder, object_ptr, "");
}

LLVMValueRef gendesc_fielddesc(compile_t* c, LLVMValueRef field_info)
{
  return LLVMBuildExtractValue(c->builder, field_info, 1, "");
}

LLVMValueRef gendesc_isnominal(compile_t* c, LLVMValueRef desc, ast_t* type)
{
  ast_t* def = (ast_t*)ast_data(type);

  switch(ast_id(def))
  {
    case TK_STRUCT:
      // The type system has ensured that at this stage the operand must be
      // the pattern type.
      return LLVMConstInt(c->i1, 1, false);

    case TK_INTERFACE:
    case TK_TRAIT:
      return gendesc_istrait(c, desc, type);

    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
      return gendesc_isentity(c, desc, type);

    default: {}
  }

  pony_assert(0);
  return GEN_NOVALUE;
}

LLVMValueRef gendesc_istrait(compile_t* c, LLVMValueRef desc, ast_t* type)
{
  reach_type_t* t = reach_type(c->reach, type);
  pony_assert(t != NULL);
  LLVMValueRef trait_id = LLVMConstInt(c->intptr, t->type_id, false);

  LLVMValueRef shift;
  LLVMValueRef mask;
  if(target_is_ilp32(c->opt->triple))
  {
    shift = LLVMConstInt(c->intptr, 5, false);
    mask = LLVMConstInt(c->intptr, 31, false);
  } else {
    shift = LLVMConstInt(c->intptr, 6, false);
    mask = LLVMConstInt(c->intptr, 63, false);
  }

  shift = LLVMConstLShr(trait_id, shift);
  mask = LLVMConstAnd(trait_id, mask);
  mask = LLVMConstShl(LLVMConstInt(c->intptr, 1, false), mask);

  LLVMValueRef bitmap = desc_field(c, desc, DESC_TRAITS);

  LLVMValueRef args[2];
  args[0] = LLVMConstInt(c->intptr, 0, false);
  args[1] = shift;

  LLVMValueRef index = LLVMBuildInBoundsGEP(c->builder, bitmap, args, 2, "");
  index = LLVMBuildLoad(c->builder, index, "");

  LLVMValueRef has_trait = LLVMBuildAnd(c->builder, index, mask, "");
  has_trait = LLVMBuildICmp(c->builder, LLVMIntNE, has_trait,
    LLVMConstInt(c->intptr, 0, false), "");

  return has_trait;
}

LLVMValueRef gendesc_isentity(compile_t* c, LLVMValueRef desc, ast_t* type)
{
  reach_type_t* t = reach_type(c->reach, type);

  if(t == NULL)
    return GEN_NOVALUE;

  compile_type_t* c_t = (compile_type_t*)t->c_type;
  LLVMValueRef dptr = LLVMBuildBitCast(c->builder, c_t->desc, c->descriptor_ptr,
    "");
  return LLVMBuildICmp(c->builder, LLVMIntEQ, desc, dptr, "");
}
