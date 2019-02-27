#include "gentype.h"
#include "genbox.h"
#include "gendesc.h"
#include "genexpr.h"
#include "genfun.h"
#include "genident.h"
#include "genname.h"
#include "genopt.h"
#include "genprim.h"
#include "genreference.h"
#include "genserialise.h"
#include "gentrace.h"
#include "../ast/id.h"
#include "../pkg/package.h"
#include "../type/cap.h"
#include "../type/reify.h"
#include "../type/subtype.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <stdlib.h>
#include <string.h>

#if PONY_LLVM >= 600
#include <llvm-c/DebugInfo.h>
#endif

void get_fieldinfo(ast_t* l_type, ast_t* right, ast_t** l_def,
  ast_t** field, uint32_t* index)
{
  ast_t* d = (ast_t*)ast_data(l_type);
  ast_t* f = ast_get(d, ast_name(right), NULL);
  uint32_t i = (uint32_t)ast_index(f);

  *l_def = d;
  *field = f;
  *index = i;
}

static void compile_type_free(void* p)
{
  POOL_FREE(compile_type_t, p);
}

static void allocate_compile_types(compile_t* c)
{
  size_t i = HASHMAP_BEGIN;
  reach_type_t* t;

  while((t = reach_types_next(&c->reach->types, &i)) != NULL)
  {
    compile_type_t* c_t = POOL_ALLOC(compile_type_t);
    memset(c_t, 0, sizeof(compile_type_t));
    c_t->free_fn = compile_type_free;
    t->c_type = (compile_opaque_t*)c_t;

    genfun_allocate_compile_methods(c, t);
  }
}

static bool make_opaque_struct(compile_t* c, reach_type_t* t)
{
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  switch(ast_id(t->ast))
  {
    case TK_NOMINAL:
    {
      switch(t->underlying)
      {
        case TK_INTERFACE:
        case TK_TRAIT:
          c_t->use_type = c->object_ptr;
          c_t->mem_type = c->object_ptr;
          return true;

        default: {}
      }

      // Find the primitive type, if there is one.
      AST_GET_CHILDREN(t->ast, pkg, id);
      const char* package = ast_name(pkg);
      const char* name = ast_name(id);

      char* triple = c->opt->triple;
      bool ilp32 = target_is_ilp32(triple);
      bool llp64 = target_is_llp64(triple);
      bool lp64 = target_is_lp64(triple);

      if(package == c->str_builtin)
      {
        if(name == c->str_I8)
          c_t->primitive = c->i8;
        else if(name == c->str_U8)
          c_t->primitive = c->i8;
        else if(name == c->str_I16)
          c_t->primitive = c->i16;
        else if(name == c->str_U16)
          c_t->primitive = c->i16;
        else if(name == c->str_I32)
          c_t->primitive = c->i32;
        else if(name == c->str_U32)
          c_t->primitive = c->i32;
        else if(name == c->str_I64)
          c_t->primitive = c->i64;
        else if(name == c->str_U64)
          c_t->primitive = c->i64;
        else if(name == c->str_I128)
          c_t->primitive = c->i128;
        else if(name == c->str_U128)
          c_t->primitive = c->i128;
        else if(ilp32 && name == c->str_ILong)
          c_t->primitive = c->i32;
        else if(ilp32 && name == c->str_ULong)
          c_t->primitive = c->i32;
        else if(ilp32 && name == c->str_ISize)
          c_t->primitive = c->i32;
        else if(ilp32 && name == c->str_USize)
          c_t->primitive = c->i32;
        else if(lp64 && name == c->str_ILong)
          c_t->primitive = c->i64;
        else if(lp64 && name == c->str_ULong)
          c_t->primitive = c->i64;
        else if(lp64 && name == c->str_ISize)
          c_t->primitive = c->i64;
        else if(lp64 && name == c->str_USize)
          c_t->primitive = c->i64;
        else if(llp64 && name == c->str_ILong)
          c_t->primitive = c->i32;
        else if(llp64 && name == c->str_ULong)
          c_t->primitive = c->i32;
        else if(llp64 && name == c->str_ISize)
          c_t->primitive = c->i64;
        else if(llp64 && name == c->str_USize)
          c_t->primitive = c->i64;
        else if(name == c->str_F32)
          c_t->primitive = c->f32;
        else if(name == c->str_F64)
          c_t->primitive = c->f64;
        else if(name == c->str_Bool)
        {
          c_t->primitive = c->i1;
          c_t->use_type = c->i1;

          // The memory representation of Bool is 32 bits wide on Darwin PPC32,
          // 8 bits wide everywhere else.
          if(target_is_ppc(triple) && ilp32 && target_is_macosx(triple))
            c_t->mem_type = c->i32;
          else
            c_t->mem_type = c->i8;

          return true;
        }
        else if(name == c->str_Pointer)
        {
          c_t->use_type = c->void_ptr;
          c_t->mem_type = c->void_ptr;
          return true;
        }
        else if(name == c->str_NullablePointer)
        {
          c_t->use_type = c->void_ptr;
          c_t->mem_type = c->void_ptr;
          return true;
        }
      }

      if(t->bare_method == NULL)
      {
        c_t->structure = LLVMStructCreateNamed(c->context, t->name);
        c_t->structure_ptr = LLVMPointerType(c_t->structure, 0);

        if(c_t->primitive != NULL)
          c_t->use_type = c_t->primitive;
        else
          c_t->use_type = c_t->structure_ptr;

        c_t->mem_type = c_t->use_type;
      } else {
        c_t->structure = c->void_ptr;
        c_t->structure_ptr = c->void_ptr;
        c_t->use_type = c->void_ptr;
        c_t->mem_type = c->void_ptr;
      }

      return true;
    }

    case TK_TUPLETYPE:
      c_t->primitive = LLVMStructCreateNamed(c->context, t->name);
      c_t->use_type = c_t->primitive;
      c_t->mem_type = c_t->primitive;
      return true;

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
      // Just a raw object pointer.
      c_t->use_type = c->object_ptr;
      c_t->mem_type = c->object_ptr;
      return true;

    default: {}
  }

  pony_assert(0);
  return false;
}

static void make_debug_basic(compile_t* c, reach_type_t* t)
{
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  uint64_t size = LLVMABISizeOfType(c->target_data, c_t->primitive);
  uint64_t align = LLVMABIAlignmentOfType(c->target_data, c_t->primitive);
  unsigned encoding;

  if(is_bool(t->ast))
  {
    encoding = DW_ATE_boolean;
  } else if(is_float(t->ast)) {
    encoding = DW_ATE_float;
  } else if(is_signed(t->ast)) {
    encoding = DW_ATE_signed;
  } else {
    encoding = DW_ATE_unsigned;
  }

#if PONY_LLVM >= 800
  (void)align;
  c_t->di_type = LLVMDIBuilderCreateBasicType(c->di, t->name, strlen(t->name),
    8 * size, encoding, LLVMDIFlagZero);
#elif PONY_LLVM >= 700
  (void)align;
  c_t->di_type = LLVMDIBuilderCreateBasicType(c->di, t->name, strlen(t->name),
    8 * size, encoding);
#else
  c_t->di_type = LLVMDIBuilderCreateBasicType(c->di, t->name,
    8 * size, 8 * align, encoding);
#endif
}

static void make_debug_prototype(compile_t* c, reach_type_t* t)
{
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  c_t->di_type = LLVMDIBuilderCreateReplaceableStruct(c->di,
    t->name, c->di_unit, c_t->di_file, (unsigned)ast_line(t->ast));

  if(t->underlying != TK_TUPLETYPE)
  {
    c_t->di_type_embed = c_t->di_type;
#if PONY_LLVM >= 700
    uint64_t size_bytes = LLVMABISizeOfType(c->target_data, c_t->mem_type);
    uint32_t align_bytes = LLVMABIAlignmentOfType(c->target_data, c_t->mem_type);

    c_t->di_type = LLVMDIBuilderCreatePointerType(c->di, c_t->di_type_embed,
      size_bytes * 8, align_bytes * 8, 0, 0, 0);
#else
    c_t->di_type = LLVMDIBuilderCreatePointerType(c->di, c_t->di_type_embed, 0,
      0);
#endif
  }
}

static void make_debug_info(compile_t* c, reach_type_t* t)
{
  ast_t* def = (ast_t*)ast_data(t->ast);
  source_t* source;

  if(def != NULL)
    source = ast_source(def);
  else
    source = ast_source(t->ast);

  const char* file = source->file;
  if(file == NULL)
    file = "";

  compile_type_t* c_t = (compile_type_t*)t->c_type;
#if PONY_LLVM < 600
  c_t->di_file = LLVMDIBuilderCreateFile(c->di, file);
#else
  c_t->di_file = LLVMDIBuilderCreateFile(c->di, file, strlen(file), "", 0);
#endif

  switch(t->underlying)
  {
    case TK_TUPLETYPE:
    case TK_STRUCT:
      make_debug_prototype(c, t);
      return;

    case TK_PRIMITIVE:
    {
      if(c_t->primitive != NULL)
        make_debug_basic(c, t);
      else
        make_debug_prototype(c, t);
      return;
    }

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_INTERFACE:
    case TK_TRAIT:
    case TK_CLASS:
    case TK_ACTOR:
      make_debug_prototype(c, t);
      return;

    default: {}
  }

  pony_assert(0);
}

static void make_box_type(compile_t* c, reach_type_t* t)
{
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  if(c_t->primitive == NULL)
    return;

  const char* box_name = genname_box(t->name);
  c_t->structure = LLVMStructCreateNamed(c->context, box_name);

  LLVMTypeRef elements[2];
  elements[0] = LLVMPointerType(c_t->desc_type, 0);
  elements[1] = c_t->mem_type;
  LLVMStructSetBody(c_t->structure, elements, 2, false);

  c_t->structure_ptr = LLVMPointerType(c_t->structure, 0);
}

static void make_global_instance(compile_t* c, reach_type_t* t)
{
  // Not a primitive type.
  if(t->underlying != TK_PRIMITIVE)
    return;

  compile_type_t* c_t = (compile_type_t*)t->c_type;

  // No instance for machine word types.
  if(c_t->primitive != NULL)
    return;

  if(t->bare_method != NULL)
    return;

  // Create a unique global instance.
  const char* inst_name = genname_instance(t->name);
  LLVMValueRef value = LLVMConstNamedStruct(c_t->structure, &c_t->desc, 1);
  c_t->instance = LLVMAddGlobal(c->module, c_t->structure, inst_name);
  LLVMSetInitializer(c_t->instance, value);
  LLVMSetGlobalConstant(c_t->instance, true);
  LLVMSetLinkage(c_t->instance, LLVMPrivateLinkage);
}

static void make_dispatch(compile_t* c, reach_type_t* t)
{
  // Do nothing if we're not an actor.
  if(t->underlying != TK_ACTOR)
    return;

  // Create a dispatch function.
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  const char* dispatch_name = genname_dispatch(t->name);
  c_t->dispatch_fn = codegen_addfun(c, dispatch_name, c->dispatch_type, true);
  LLVMSetFunctionCallConv(c_t->dispatch_fn, LLVMCCallConv);
  LLVMSetLinkage(c_t->dispatch_fn, LLVMExternalLinkage);
  codegen_startfun(c, c_t->dispatch_fn, NULL, NULL, NULL, false);

  LLVMBasicBlockRef unreachable = codegen_block(c, "unreachable");

  // Read the message ID.
  LLVMValueRef msg = LLVMGetParam(c_t->dispatch_fn, 2);
  LLVMValueRef id_ptr = LLVMBuildStructGEP(c->builder, msg, 1, "");
  LLVMValueRef id = LLVMBuildLoad(c->builder, id_ptr, "id");

  // Store a reference to the dispatch switch. When we build behaviours, we
  // will add cases to this switch statement based on message ID.
  c_t->dispatch_switch = LLVMBuildSwitch(c->builder, id, unreachable, 0);

  // Mark the default case as unreachable.
  LLVMPositionBuilderAtEnd(c->builder, unreachable);

  // Workaround for LLVM's "infinite loops are undefined behaviour".
  // If a Pony behaviour contains an infinite loop, the LLVM optimiser in its
  // current state can assume that the associated message is never received.
  // From there, if the dispatch switch is optimised to a succession of
  // conditional branches on the message ID, it is very likely that receiving
  // the optimised-out message will call another behaviour on the actor, which
  // is very very bad.
  // This inline assembly cannot be analysed by the optimiser (and thus must be
  // assumed to have side-effects), which prevents the removal of the default
  // case, which in turn prevents the replacement of the switch. In addition,
  // the setup in codegen_machine results in unreachable instructions being
  // lowered to trapping machine instructions (e.g. ud2 on x86), which are
  // guaranteed to crash the program.
  // As a result, if an actor receives a message affected by this bug, the
  // program will crash immediately instead of doing some crazy stuff.
  // TODO: Remove this when LLVM properly supports infinite loops.
  LLVMTypeRef void_fn = LLVMFunctionType(c->void_type, NULL, 0, false);
  LLVMValueRef asmstr = LLVMConstInlineAsm(void_fn, "", "~{memory}", true,
    false);
  LLVMBuildCall(c->builder, asmstr, NULL, 0, "");
  LLVMBuildUnreachable(c->builder);

  codegen_finishfun(c);
}

static bool make_struct(compile_t* c, reach_type_t* t)
{
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  LLVMTypeRef type;
  int extra = 0;
  bool packed = false;

  if(t->bare_method != NULL)
    return true;

  switch(t->underlying)
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_INTERFACE:
    case TK_TRAIT:
      return true;

    case TK_TUPLETYPE:
      type = c_t->primitive;
      break;

    case TK_STRUCT:
    {
      // Pointer and NullablePointer will have no structure.
      if(c_t->structure == NULL)
        return true;

      type = c_t->structure;
      ast_t* def = (ast_t*)ast_data(t->ast);
      if(ast_has_annotation(def, "packed"))
        packed = true;

      break;
    }

    case TK_PRIMITIVE:
      // Machine words will have a primitive.
      if(c_t->primitive != NULL)
      {
        // The ABI size for machine words and tuples is the boxed size.
        c_t->abi_size = (size_t)LLVMABISizeOfType(c->target_data,
          c_t->structure);
        return true;
      }

      extra = 1;
      type = c_t->structure;
      break;

    case TK_CLASS:
      extra = 1;
      type = c_t->structure;
      break;

    case TK_ACTOR:
      extra = 2;
      type = c_t->structure;
      break;

    default:
      pony_assert(0);
      return false;
  }

  size_t buf_size = (t->field_count + extra) * sizeof(LLVMTypeRef);
  LLVMTypeRef* elements = (LLVMTypeRef*)ponyint_pool_alloc_size(buf_size);

  // Create the type descriptor as element 0.
  if(extra > 0)
    elements[0] = LLVMPointerType(c_t->desc_type, 0);

  // Create the actor pad as element 1.
  if(extra > 1)
    elements[1] = c->actor_pad;

  for(uint32_t i = 0; i < t->field_count; i++)
  {
    compile_type_t* f_c_t = (compile_type_t*)t->fields[i].type->c_type;

    if(t->fields[i].embed)
      elements[i + extra] = f_c_t->structure;
    else
      elements[i + extra] = f_c_t->mem_type;

    if(elements[i + extra] == NULL)
    {
      pony_assert(0);
      return false;
    }
  }

  LLVMStructSetBody(type, elements, t->field_count + extra, packed);
  ponyint_pool_free_size(buf_size, elements);
  return true;
}

static LLVMMetadataRef make_debug_field(compile_t* c, reach_type_t* t,
  uint32_t i)
{
  const char* name;
  char buf[32];
#if PONY_LLVM >= 700
  LLVMDIFlags flags = LLVMDIFlagZero;
#else
  unsigned flags = 0;
#endif
  uint64_t offset = 0;
  ast_t* ast;
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  if(t->underlying != TK_TUPLETYPE)
  {
    ast_t* def = (ast_t*)ast_data(t->ast);
    ast_t* members = ast_childidx(def, 4);
    ast = ast_childidx(members, i);
    name = ast_name(ast_child(ast));

    if(is_name_private(name))
#if PONY_LLVM >= 700
      flags = (LLVMDIFlags)(flags | LLVMDIFlagPrivate);
#else
      flags |= DW_FLAG_Private;
#endif

    uint32_t extra = 0;

    if(t->underlying != TK_STRUCT)
      extra++;

    if(t->underlying == TK_ACTOR)
      extra++;

    offset = LLVMOffsetOfElement(c->target_data, c_t->structure, i + extra);
  } else {
    snprintf(buf, 32, "_%d", i + 1);
    name = buf;
    ast = t->ast;
    offset = LLVMOffsetOfElement(c->target_data, c_t->primitive, i);
  }

  LLVMTypeRef type;
  LLVMMetadataRef di_type;
  compile_type_t* f_c_t = (compile_type_t*)t->fields[i].type->c_type;

  if(t->fields[i].embed)
  {
    type = f_c_t->structure;
    di_type = f_c_t->di_type_embed;
  } else {
    type = f_c_t->mem_type;
    di_type = f_c_t->di_type;
  }

  uint64_t size = LLVMABISizeOfType(c->target_data, type);
  uint64_t align = LLVMABIAlignmentOfType(c->target_data, type);

#if PONY_LLVM >= 700
  return LLVMDIBuilderCreateMemberType(c->di, c->di_unit, name, strlen(name),
    c_t->di_file, (unsigned)ast_line(ast), 8 * size, (uint32_t)(8 * align),
    8 * offset, flags, di_type);
#else
  return LLVMDIBuilderCreateMemberType(c->di, c->di_unit, name, c_t->di_file,
    (unsigned)ast_line(ast), 8 * size, 8 * align, 8 * offset, flags, di_type);
#endif
}

static void make_debug_fields(compile_t* c, reach_type_t* t)
{
  LLVMMetadataRef* fields = NULL;
  size_t fields_buf_size = 0;

  if(t->field_count > 0)
  {
    fields_buf_size = t->field_count * sizeof(LLVMMetadataRef);
    fields = (LLVMMetadataRef*)ponyint_pool_alloc_size(fields_buf_size);

    for(uint32_t i = 0; i < t->field_count; i++)
      fields[i] = make_debug_field(c, t, i);
  }

  LLVMTypeRef type;
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  if(t->underlying != TK_TUPLETYPE)
    type = c_t->structure;
  else
    type = c_t->primitive;

  uint64_t size = 0;
  uint64_t align = 0;

  if(type != NULL)
  {
    size = LLVMABISizeOfType(c->target_data, type);
    align = LLVMABIAlignmentOfType(c->target_data, type);
  }

#if PONY_LLVM >= 700
  LLVMMetadataRef di_type = LLVMDIBuilderCreateStructType(c->di, c->di_unit,
    t->name, strlen(t->name), c_t->di_file, (unsigned) ast_line(t->ast),
    8 * size, (uint32_t)(8 * align), LLVMDIFlagZero, 0, fields, t->field_count,
    0, 0, t->name, strlen(t->name));
#else
  LLVMMetadataRef fields_array = NULL;
  if(fields != NULL)
    fields_array = LLVMDIBuilderGetOrCreateArray(c->di, fields, t->field_count);

  LLVMMetadataRef di_type = LLVMDIBuilderCreateStructType(c->di, c->di_unit,
    t->name, c_t->di_file, (unsigned) ast_line(t->ast), 8 * size, 8 * align,
    fields_array);
#endif

  if(fields != NULL)
    ponyint_pool_free_size(fields_buf_size, fields);

  if(t->underlying != TK_TUPLETYPE)
  {
    LLVMMetadataReplaceAllUsesWith(c_t->di_type_embed, di_type);
    c_t->di_type_embed = di_type;
  } else {
    LLVMMetadataReplaceAllUsesWith(c_t->di_type, di_type);
    c_t->di_type = di_type;
  }
}

static void make_debug_final(compile_t* c, reach_type_t* t)
{
  switch(t->underlying)
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    case TK_INTERFACE:
    case TK_TRAIT:
    case TK_STRUCT:
    case TK_CLASS:
    case TK_ACTOR:
      make_debug_fields(c, t);
      return;

    case TK_PRIMITIVE:
    {
      compile_type_t* c_t = (compile_type_t*)t->c_type;
      if(c_t->primitive == NULL)
        make_debug_fields(c, t);
      return;
    }

    default: {}
  }

  pony_assert(0);
}

static void make_intrinsic_methods(compile_t* c, reach_type_t* t)
{
  if(t->can_be_boxed)
  {
    gen_digestof_fun(c, t);
    if(ast_id(t->ast) == TK_TUPLETYPE)
      gen_is_tuple_fun(c, t);
  }

  if(ast_id(t->ast) != TK_NOMINAL)
    return;

  // Find the primitive type, if there is one.
  AST_GET_CHILDREN(t->ast, pkg, id);
  const char* package = ast_name(pkg);
  const char* name = ast_name(id);

  if(package == c->str_builtin)
  {
    if(name == c->str_Pointer)
      genprim_pointer_methods(c, t);
    else if(name == c->str_NullablePointer)
      genprim_nullable_pointer_methods(c, t);
    else if(name == c->str_DoNotOptimise)
      genprim_donotoptimise_methods(c, t);
    else if(name == c->str_Platform)
      genprim_platform_methods(c, t);
  }
}

static bool make_trace(compile_t* c, reach_type_t* t)
{
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  if(c_t->trace_fn == NULL)
    return true;

  if(t->underlying == TK_CLASS)
  {
    // Special case the array trace function.
    AST_GET_CHILDREN(t->ast, pkg, id);
    const char* package = ast_name(pkg);
    const char* name = ast_name(id);

    if((package == c->str_builtin) && (name == c->str_Array))
    {
      genprim_array_trace(c, t);
      return true;
    }
  }

  // Generate the trace function.
  codegen_startfun(c, c_t->trace_fn, NULL, NULL, NULL, false);
  LLVMSetFunctionCallConv(c_t->trace_fn, LLVMCCallConv);
  LLVMSetLinkage(c_t->trace_fn, LLVMExternalLinkage);

  LLVMValueRef ctx = LLVMGetParam(c_t->trace_fn, 0);
  LLVMValueRef arg = LLVMGetParam(c_t->trace_fn, 1);
  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, c_t->structure_ptr,
    "object");

  int extra = 0;

  switch(t->underlying)
  {
    case TK_CLASS:
      extra = 1; // Type descriptor.
      break;

    case TK_ACTOR:
      extra = 2; // Type descriptor and pad.
      break;

    case TK_TUPLETYPE:
      // Skip over the box's descriptor now. It avoids multi-level GEPs later.
      object = LLVMBuildStructGEP(c->builder, object, 1, "");
      break;

    default: {}
  }

  for(uint32_t i = 0; i < t->field_count; i++)
  {
    reach_field_t* f = &t->fields[i];
    compile_type_t* f_c_t = (compile_type_t*)f->type->c_type;
    LLVMValueRef field = LLVMBuildStructGEP(c->builder, object, i + extra, "");

    if(!f->embed)
    {
      // Call the trace function indirectly depending on rcaps.
      field = LLVMBuildLoad(c->builder, field, "");
      ast_t* field_type = f->ast;
      field = gen_assign_cast(c, f_c_t->use_type, field, field_type);
      gentrace(c, ctx, field, field, field_type, field_type);
    } else {
      // Call the trace function directly without marking the field.
      LLVMValueRef trace_fn = f_c_t->trace_fn;

      if(trace_fn != NULL)
      {
        LLVMValueRef args[2];
        args[0] = ctx;
        args[1] = LLVMBuildBitCast(c->builder, field, c->object_ptr, "");

        LLVMBuildCall(c->builder, trace_fn, args, 2, "");
      }
    }
  }

  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);
  return true;
}

bool gentypes(compile_t* c)
{
  reach_type_t* t;
  size_t i;

  if(target_is_ilp32(c->opt->triple))
    c->trait_bitmap_size = ((c->reach->trait_type_count + 31) & ~31) >> 5;
  else
    c->trait_bitmap_size = ((c->reach->trait_type_count + 63) & ~63) >> 6;

  allocate_compile_types(c);
  genprim_builtins(c);

  if(c->opt->verbosity >= VERBOSITY_INFO)
    fprintf(stderr, " Data prototypes\n");

  i = HASHMAP_BEGIN;

  while((t = reach_types_next(&c->reach->types, &i)) != NULL)
  {
    if(!make_opaque_struct(c, t))
      return false;

    gendesc_type(c, t);
    make_debug_info(c, t);
    make_box_type(c, t);
    make_dispatch(c, t);
    gentrace_prototype(c, t);
  }

  gendesc_table(c);

  c->numeric_sizes = gen_numeric_size_table(c);

  if(c->opt->verbosity >= VERBOSITY_INFO)
    fprintf(stderr, " Data types\n");

  i = HASHMAP_BEGIN;

  while((t = reach_types_next(&c->reach->types, &i)) != NULL)
  {
    if(!make_struct(c, t))
      return false;

    make_global_instance(c, t);
  }

  genprim_signature(c);

  // Cache the instance of None, which is used as the return value for
  // behaviour calls.
  t = reach_type_name(c->reach, "None");
  pony_assert(t != NULL);
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  c->none_instance = c_t->instance;

  if(c->opt->verbosity >= VERBOSITY_INFO)
    fprintf(stderr, " Function prototypes\n");

  i = HASHMAP_BEGIN;

  while((t = reach_types_next(&c->reach->types, &i)) != NULL)
  {
    compile_type_t* c_t = (compile_type_t*)t->c_type;

    // The ABI size for machine words and tuples is the boxed size.
    if(c_t->structure != NULL)
      c_t->abi_size = (size_t)LLVMABISizeOfType(c->target_data, c_t->structure);

    make_debug_final(c, t);
    make_intrinsic_methods(c, t);

    if(!genfun_method_sigs(c, t))
      return false;
  }

  if(c->opt->verbosity >= VERBOSITY_INFO)
    fprintf(stderr, " Functions\n");

  i = HASHMAP_BEGIN;

  while((t = reach_types_next(&c->reach->types, &i)) != NULL)
  {
    if(!genfun_method_bodies(c, t))
      return false;
  }

  genfun_primitive_calls(c);

  if(c->opt->verbosity >= VERBOSITY_INFO)
    fprintf(stderr, " Descriptors\n");

  i = HASHMAP_BEGIN;

  while((t = reach_types_next(&c->reach->types, &i)) != NULL)
  {
    if(!make_trace(c, t))
      return false;

    if(!genserialise(c, t))
      return false;

    gendesc_init(c, t);
  }

  return true;
}
