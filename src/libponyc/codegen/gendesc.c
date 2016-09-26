#include "gendesc.h"
#include "genname.h"
#include "gentype.h"
#include "../type/reify.h"
#include "../ast/stringtab.h"
#include "../../libponyrt/mem/pool.h"
#include <string.h>
#include <assert.h>

#define DESC_ID 0
#define DESC_SIZE 1
#define DESC_TRAIT_COUNT 2
#define DESC_FIELD_COUNT 3
#define DESC_FIELD_OFFSET 4
#define DESC_INSTANCE 5
#define DESC_TRACE 6
#define DESC_SERIALISE_TRACE 7
#define DESC_SERIALISE 8
#define DESC_DESERIALISE 9
#define DESC_DISPATCH 10
#define DESC_FINALISE 11
#define DESC_EVENT_NOTIFY 12
#define DESC_TRAITS 13
#define DESC_FIELDS 14
#define DESC_VTABLE 15

#define DESC_LENGTH 16

static LLVMValueRef make_unbox_function(compile_t* c, reach_type_t* t,
  reach_method_t* m)
{
  // Create a new unboxing function that forwards to the real function.
  LLVMTypeRef f_type = LLVMGetElementType(LLVMTypeOf(m->func));
  int count = LLVMCountParamTypes(f_type);

  // Leave space for a receiver if it's a constructor vtable entry.
  size_t buf_size = (count + 1) * sizeof(LLVMTypeRef);
  LLVMTypeRef* params = (LLVMTypeRef*)ponyint_pool_alloc_size(buf_size);
  LLVMGetParamTypes(f_type, params);
  LLVMTypeRef ret_type = LLVMGetReturnType(f_type);

  const char* box_name = genname_box(t->name);
  const char* unbox_name = genname_unbox(m->full_name);

  if(ast_id(m->r_fun) != TK_NEW)
  {
    // It's the same type, but it takes the boxed type instead of the primitive
    // type as the receiver.
    params[0] = t->structure_ptr;
  } else {
    // For a constructor, the unbox_fun has a receiver, even though the real
    // method does not.
    memmove(&params[1], &params[0], count * sizeof(LLVMTypeRef*));
    params[0] = t->structure_ptr;
    count++;
  }

  LLVMTypeRef unbox_type = LLVMFunctionType(ret_type, params, count, false);
  LLVMValueRef unbox_fun = codegen_addfun(c, unbox_name, unbox_type);
  codegen_startfun(c, unbox_fun, NULL, NULL);

  // Extract the primitive type from element 1 and call the real function.
  LLVMValueRef this_ptr = LLVMGetParam(unbox_fun, 0);
  LLVMValueRef primitive_ptr = LLVMBuildStructGEP(c->builder, this_ptr, 1, "");
  LLVMValueRef primitive = LLVMBuildLoad(c->builder, primitive_ptr, "");

  LLVMValueRef metadata = tbaa_metadata_for_box_type(c, box_name);
  const char id[] = "tbaa";
  LLVMSetMetadata(primitive, LLVMGetMDKindID(id, sizeof(id) - 1), metadata);

  LLVMValueRef* args = (LLVMValueRef*)ponyint_pool_alloc_size(buf_size);

  if(ast_id(m->r_fun) != TK_NEW)
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

  LLVMValueRef result = codegen_call(c, m->func, args, count);
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

static int cmp_uint32(const void* elem1, const void* elem2)
{
  uint32_t a = *((uint32_t*)elem1);
  uint32_t b = *((uint32_t*)elem2);

  if(a < b)
    return -1;

  if(a > b)
    return 1;

  return 0;
}

static size_t unique_uint32(uint32_t* list, size_t len)
{
  uint32_t* first = list;
  uint32_t* last = list + len;
  uint32_t* r = list;

  while(++first != last)
  {
    if(!(*r == *first))
      *(++r) = *first;
  }

  return (++r) - list;
}

static uint32_t trait_count(reach_type_t* t, uint32_t** list,
  size_t* list_size)
{
  switch(t->underlying)
  {
    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
    {
      uint32_t count = (uint32_t)reach_type_cache_size(&t->subtypes);

      if(count == 0)
        return 0;

      // Sort the trait identifiers.
      size_t tid_size = count * sizeof(uint32_t);
      uint32_t* tid = (uint32_t*)ponyint_pool_alloc_size(tid_size);

      size_t i = HASHMAP_BEGIN;
      size_t index = 0;
      reach_type_t* provide;

      while((provide = reach_type_cache_next(&t->subtypes, &i)) != NULL)
        tid[index++] = provide->type_id;

      qsort(tid, index, sizeof(uint32_t), cmp_uint32);
      count = (uint32_t)unique_uint32(tid, index);

      if(list != NULL)
      {
        *list = tid;
        *list_size = tid_size;
      } else {
        ponyint_pool_free_size(tid_size, tid);
      }

      return count;
    }

    default: {}
  }

  return 0;
}

static LLVMValueRef make_trait_list(compile_t* c, reach_type_t* t,
  uint32_t* final_count)
{
  // The list is an array of integers.
  uint32_t* tid;
  size_t tid_size;
  uint32_t count = trait_count(t, &tid, &tid_size);

  // If we have no traits, return a null pointer to a list.
  if(count == 0)
    return LLVMConstNull(LLVMPointerType(LLVMArrayType(c->i32, 0), 0));

  // Create a constant array of trait identifiers.
  size_t list_size = count * sizeof(LLVMValueRef);
  LLVMValueRef* list = (LLVMValueRef*)ponyint_pool_alloc_size(list_size);

  for(uint32_t i = 0; i < count; i++)
    list[i] = LLVMConstInt(c->i32, tid[i], false);

  LLVMValueRef trait_array = LLVMConstArray(c->i32, list, count);

  // Create a global to hold the array.
  const char* name = genname_traitlist(t->name);
  LLVMTypeRef list_type = LLVMArrayType(c->i32, count);
  LLVMValueRef global = LLVMAddGlobal(c->module, list_type, name);
  LLVMSetGlobalConstant(global, true);
  LLVMSetLinkage(global, LLVMPrivateLinkage);
  LLVMSetInitializer(global, trait_array);

  ponyint_pool_free_size(tid_size, tid);
  ponyint_pool_free_size(list_size, list);

  *final_count = count;
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

  return LLVMConstInt(c->i32,
    LLVMOffsetOfElement(c->target_data, t->structure, index), false);
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

  for(uint32_t i = 0; i < count; i++)
  {
    LLVMValueRef fdesc[2];
    fdesc[0] = LLVMConstInt(c->i32,
      LLVMOffsetOfElement(c->target_data, t->primitive, i), false);

    if(t->fields[i].type->desc != NULL)
    {
      // We are a concrete type.
      fdesc[1] = LLVMConstBitCast(t->fields[i].type->desc,
        c->descriptor_ptr);
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

  size_t i = HASHMAP_BEGIN;
  reach_method_name_t* n;

  while((n = reach_method_names_next(&t->methods, &i)) != NULL)
  {
    size_t j = HASHMAP_BEGIN;
    reach_method_t* m;

    while((m = reach_mangled_next(&n->r_mangled, &j)) != NULL)
    {
      uint32_t index = m->vtable_index;
      assert(index != (uint32_t)-1);
      assert(vtable[index] == NULL);

      if(t->primitive != NULL)
        vtable[index] = make_unbox_function(c, t, m);
      else
        vtable[index] = make_desc_ptr(m->func, c->void_ptr);
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
  params[DESC_TRAIT_COUNT] = c->i32;
  params[DESC_FIELD_COUNT] = c->i32;
  params[DESC_FIELD_OFFSET] = c->i32;
  params[DESC_INSTANCE] = c->object_ptr;
  params[DESC_TRACE] = c->trace_fn;
  params[DESC_SERIALISE_TRACE] = c->trace_fn;
  params[DESC_SERIALISE] = c->serialise_fn;
  params[DESC_DESERIALISE] = c->trace_fn;
  params[DESC_DISPATCH] = c->dispatch_fn;
  params[DESC_FINALISE] = c->final_fn;
  params[DESC_EVENT_NOTIFY] = c->i32;
  params[DESC_TRAITS] = LLVMPointerType(LLVMArrayType(c->i32, 0), 0);
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
  uint32_t traits = trait_count(t, NULL, NULL);
  uint32_t fields = 0;
  uint32_t vtable_size = 0;

  if(t->underlying == TK_TUPLETYPE)
    fields = t->field_count;
  else
    vtable_size = t->vtable_size;

  t->desc_type = LLVMStructCreateNamed(c->context, desc_name);
  LLVMTypeRef params[DESC_LENGTH];

  params[DESC_ID] = c->i32;
  params[DESC_SIZE] = c->i32;
  params[DESC_TRAIT_COUNT] = c->i32;
  params[DESC_FIELD_COUNT] = c->i32;
  params[DESC_FIELD_OFFSET] = c->i32;
  params[DESC_INSTANCE] = c->object_ptr;
  params[DESC_TRACE] = c->trace_fn;
  params[DESC_SERIALISE_TRACE] = c->trace_fn;
  params[DESC_SERIALISE] = c->serialise_fn;
  params[DESC_DESERIALISE] = c->trace_fn;
  params[DESC_DISPATCH] = c->dispatch_fn;
  params[DESC_FINALISE] = c->final_fn;
  params[DESC_EVENT_NOTIFY] = c->i32;
  params[DESC_TRAITS] = LLVMPointerType(LLVMArrayType(c->i32, traits), 0);
  params[DESC_FIELDS] = LLVMPointerType(
    LLVMArrayType(c->field_descriptor, fields), 0);
  params[DESC_VTABLE] = LLVMArrayType(c->void_ptr, vtable_size);

  LLVMStructSetBody(t->desc_type, params, DESC_LENGTH, false);

  t->desc = LLVMAddGlobal(c->module, t->desc_type, desc_name);
  LLVMSetGlobalConstant(t->desc, true);
  LLVMSetLinkage(t->desc, LLVMPrivateLinkage);
}

void gendesc_init(compile_t* c, reach_type_t* t)
{
  if(t->desc_type == NULL)
    return;

  // Initialise the global descriptor.
  uint32_t event_notify_index = reach_vtable_index(t, c->str__event_notify);
  uint32_t trait_count = 0;
  LLVMValueRef trait_list = make_trait_list(c, t, &trait_count);

  LLVMValueRef args[DESC_LENGTH];

  args[DESC_ID] = LLVMConstInt(c->i32, t->type_id, false);
  args[DESC_SIZE] = LLVMConstInt(c->i32, t->abi_size, false);
  args[DESC_TRAIT_COUNT] = LLVMConstInt(c->i32, trait_count, false);
  args[DESC_FIELD_COUNT] = make_field_count(c, t);
  args[DESC_FIELD_OFFSET] = make_field_offset(c, t);
  args[DESC_INSTANCE] = make_desc_ptr(t->instance, c->object_ptr);
  args[DESC_TRACE] = make_desc_ptr(t->trace_fn, c->trace_fn);
  args[DESC_SERIALISE_TRACE] = make_desc_ptr(t->serialise_trace_fn,
    c->trace_fn);
  args[DESC_SERIALISE] = make_desc_ptr(t->serialise_fn, c->serialise_fn);
  args[DESC_DESERIALISE] = make_desc_ptr(t->deserialise_fn, c->trace_fn);
  args[DESC_DISPATCH] = make_desc_ptr(t->dispatch_fn, c->dispatch_fn);
  args[DESC_FINALISE] = make_desc_ptr(t->final_fn, c->final_fn);
  args[DESC_EVENT_NOTIFY] = LLVMConstInt(c->i32, event_notify_index, false);
  args[DESC_TRAITS] = trait_list;
  args[DESC_FIELDS] = make_field_list(c, t);
  args[DESC_VTABLE] = make_vtable(c, t);

  LLVMValueRef desc = LLVMConstNamedStruct(t->desc_type, args, DESC_LENGTH);
  LLVMSetInitializer(t->desc, desc);
  LLVMSetGlobalConstant(t->desc, true);
}

void gendesc_table(compile_t* c)
{
  uint32_t len = c->reach->next_type_id;
  size_t size = len * sizeof(LLVMValueRef);
  LLVMValueRef* args = (LLVMValueRef*)ponyint_pool_alloc_size(size);

  reach_type_t* t;
  size_t i = HASHMAP_BEGIN;

  while((t = reach_types_next(&c->reach->types, &i)) != NULL)
  {
    LLVMValueRef desc;

    if(t->desc != NULL)
      desc = LLVMBuildBitCast(c->builder, t->desc, c->descriptor_ptr, "");
    else
      desc = LLVMConstNull(c->descriptor_ptr);

    args[t->type_id] = desc;
  }

  LLVMTypeRef type = LLVMArrayType(c->descriptor_ptr, len);
  LLVMValueRef table = LLVMAddGlobal(c->module, type, "__DescTable");
  LLVMValueRef value = LLVMConstArray(c->descriptor_ptr, args, len);
  LLVMSetInitializer(table, value);
  LLVMSetGlobalConstant(table, true);

  LLVMValueRef table_size = LLVMAddGlobal(c->module, c->intptr,
    "__DescTableSize");
  LLVMSetInitializer(table_size, LLVMConstInt(c->intptr, len, false));
  LLVMSetGlobalConstant(table_size, true);

  ponyint_pool_free_size(size, args);
}

static LLVMValueRef desc_field(compile_t* c, LLVMValueRef desc, int index)
{
  LLVMValueRef ptr = LLVMBuildStructGEP(c->builder, desc, index, "");
  LLVMValueRef field = LLVMBuildLoad(c->builder, ptr, "");
  const char id[] = "tbaa";
  LLVMSetMetadata(field, LLVMGetMDKindID(id, sizeof(id) - 1),
    c->tbaa_descriptor);
  return field;
}

LLVMValueRef gendesc_fetch(compile_t* c, LLVMValueRef object)
{
  LLVMValueRef ptr = LLVMBuildStructGEP(c->builder, object, 0, "");
  LLVMValueRef desc = LLVMBuildLoad(c->builder, ptr, "");
  const char id[] = "tbaa";
  LLVMSetMetadata(desc, LLVMGetMDKindID(id, sizeof(id) - 1), c->tbaa_descptr);
  return desc;
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

  LLVMValueRef func_ptr = LLVMBuildInBoundsGEP(c->builder, vtable, gep, 2, "");
  LLVMValueRef fun = LLVMBuildLoad(c->builder, func_ptr, "");
  const char id[] = "tbaa";
  LLVMSetMetadata(fun, LLVMGetMDKindID(id, sizeof(id) - 1), c->tbaa_descriptor);
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
  const char id[] = "tbaa";
  LLVMSetMetadata(field_info, LLVMGetMDKindID(id, sizeof(id) - 1),
    c->tbaa_descriptor);
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

  assert(0);
  return GEN_NOVALUE;
}

LLVMValueRef gendesc_istrait(compile_t* c, LLVMValueRef desc, ast_t* type)
{
  // Get the trait identifier.
  reach_type_t* t = reach_type(c->reach, type);
  assert(t != NULL);
  LLVMValueRef trait_id = LLVMConstInt(c->i32, t->type_id, false);

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

  LLVMValueRef id_ptr = LLVMBuildInBoundsGEP(c->builder, list, gep, 2, "");
  LLVMValueRef id = LLVMBuildLoad(c->builder, id_ptr, "");
  const char node_id[] = "tbaa";
  LLVMSetMetadata(id, LLVMGetMDKindID(node_id, sizeof(node_id) - 1),
    c->tbaa_descriptor);
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

LLVMValueRef gendesc_isentity(compile_t* c, LLVMValueRef desc, ast_t* type)
{
  reach_type_t* t = reach_type(c->reach, type);

  if(t == NULL)
    return GEN_NOVALUE;

  LLVMValueRef dptr = LLVMBuildBitCast(c->builder, t->desc, c->descriptor_ptr,
    "");
  return LLVMBuildICmp(c->builder, LLVMIntEQ, desc, dptr, "");
}
