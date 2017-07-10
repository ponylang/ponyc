#include "genreference.h"
#include "genbox.h"
#include "gencall.h"
#include "gendesc.h"
#include "genexpr.h"
#include "genfun.h"
#include "genname.h"
#include "gentype.h"
#include "../expr/literal.h"
#include "../reach/subtype.h"
#include "../type/cap.h"
#include "../type/subtype.h"
#include "../type/viewpoint.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <string.h>

struct genned_string_t
{
  const char* string;
  LLVMValueRef global;
};

static size_t genned_string_hash(genned_string_t* s)
{
  return ponyint_hash_ptr(s->string);
}

static bool genned_string_cmp(genned_string_t* a, genned_string_t* b)
{
  return a->string == b->string;
}

static void genned_string_free(genned_string_t* s)
{
  POOL_FREE(genned_string_t, s);
}

DEFINE_HASHMAP(genned_strings, genned_strings_t, genned_string_t,
  genned_string_hash, genned_string_cmp, ponyint_pool_alloc_size,
  ponyint_pool_free_size, genned_string_free);

LLVMValueRef gen_this(compile_t* c, ast_t* ast)
{
  (void)ast;
  return LLVMGetParam(codegen_fun(c), 0);
}

LLVMValueRef gen_param(compile_t* c, ast_t* ast)
{
  ast_t* def = (ast_t*)ast_data(ast);
  pony_assert(def != NULL);
  int index = (int)ast_index(def);

  if(!c->frame->bare_function)
    index++;

  return LLVMGetParam(codegen_fun(c), index);
}

static LLVMValueRef make_fieldptr(compile_t* c, LLVMValueRef l_value,
  ast_t* l_type, ast_t* right)
{
  pony_assert(ast_id(l_type) == TK_NOMINAL);
  pony_assert(ast_id(right) == TK_ID);

  ast_t* def = (ast_t*)ast_data(l_type);
  ast_t* field = ast_get(def, ast_name(right), NULL);
  int index = (int)ast_index(field);

  if(ast_id(def) != TK_STRUCT)
    index++;

  if(ast_id(def) == TK_ACTOR)
    index++;

  return LLVMBuildStructGEP(c->builder, l_value, index, "");
}

LLVMValueRef gen_fieldptr(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, left, right);

  LLVMValueRef l_value = gen_expr(c, left);

  if(l_value == NULL)
    return NULL;

  ast_t* l_type = ast_type(left);
  return make_fieldptr(c, l_value, l_type, right);
}

LLVMValueRef gen_fieldload(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, left, right);
  ast_t* l_type = ast_type(left);

  LLVMValueRef field = gen_fieldptr(c, ast);

  if(field == NULL)
    return NULL;

  pony_assert(ast_id(l_type) == TK_NOMINAL);

  field = LLVMBuildLoad(c->builder, field, "");
  LLVMValueRef metadata = tbaa_metadata_for_type(c, l_type);
  const char id[] = "tbaa";
  LLVMSetMetadata(field, LLVMGetMDKindID(id, sizeof(id) - 1), metadata);

  return field;
}


LLVMValueRef gen_fieldembed(compile_t* c, ast_t* ast)
{
  LLVMValueRef field = gen_fieldptr(c, ast);

  if(field == NULL)
    return NULL;

  return field;
}

static LLVMValueRef make_tupleelemptr(compile_t* c, LLVMValueRef l_value,
  ast_t* l_type, ast_t* right)
{
  pony_assert(ast_id(l_type) == TK_TUPLETYPE);
  int index = (int)ast_int(right)->low;

  return LLVMBuildExtractValue(c->builder, l_value, index, "");
}

LLVMValueRef gen_tupleelemptr(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, left, right);

  LLVMValueRef l_value = gen_expr(c, left);

  if(l_value == NULL)
    return NULL;

  ast_t* l_type = ast_type(left);
  return make_tupleelemptr(c, l_value, l_type, right);
}

LLVMValueRef gen_tuple(compile_t* c, ast_t* ast)
{
  ast_t* child = ast_child(ast);

  if(ast_sibling(child) == NULL)
    return gen_expr(c, child);

  ast_t* type = ast_type(ast);

  // If we contain '_', we have no usable value.
  if(contains_dontcare(type))
    return GEN_NOTNEEDED;

  reach_type_t* t = reach_type(c->reach, type);
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  LLVMValueRef tuple = LLVMGetUndef(c_t->primitive);
  int i = 0;

  while(child != NULL)
  {
    LLVMValueRef value = gen_expr(c, child);

    if(value == NULL)
      return NULL;

    // We'll have an undefined element if one of our source elements is a
    // variable declaration. This is ok, since the tuple value will never be
    // used.
    if(value == GEN_NOVALUE)
      return GEN_NOTNEEDED;

    tuple = LLVMBuildInsertValue(c->builder, tuple, value, i++, "");
    child = ast_sibling(child);
  }

  return tuple;
}

LLVMValueRef gen_localdecl(compile_t* c, ast_t* ast)
{
  ast_t* id = ast_child(ast);
  ast_t* type = ast_type(id);
  const char* name = ast_name(id);

  // If this local has already been generated, don't create another copy. This
  // can happen when the same ast node is generated more than once, such as
  // the condition block of a while expression.
  LLVMValueRef value = codegen_getlocal(c, name);

  if(value != NULL)
    return GEN_NOVALUE;

  reach_type_t* t = reach_type(c->reach, type);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  // All alloca should happen in the entry block of a function.
  LLVMBasicBlockRef this_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef entry_block = LLVMGetEntryBasicBlock(codegen_fun(c));
  LLVMValueRef inst = LLVMGetFirstInstruction(entry_block);

  if(inst != NULL)
    LLVMPositionBuilderBefore(c->builder, inst);
  else
    LLVMPositionBuilderAtEnd(c->builder, entry_block);

  LLVMValueRef alloc = LLVMBuildAlloca(c->builder, c_t->use_type, name);

  // Store the alloca to use when we reference this local.
  codegen_setlocal(c, name, alloc);

  LLVMMetadataRef file = codegen_difile(c);
  LLVMMetadataRef scope = codegen_discope(c);

  LLVMMetadataRef info = LLVMDIBuilderCreateAutoVariable(c->di, scope, name,
    file, (unsigned)ast_line(ast), c_t->di_type);

  LLVMMetadataRef expr = LLVMDIBuilderCreateExpression(c->di, NULL, 0);

  LLVMDIBuilderInsertDeclare(c->di, alloc, info, expr,
    (unsigned)ast_line(ast), (unsigned)ast_pos(ast), scope,
    LLVMGetInsertBlock(c->builder));

  // Put the builder back where it was.
  LLVMPositionBuilderAtEnd(c->builder, this_block);
  return GEN_NOVALUE;
}

LLVMValueRef gen_localptr(compile_t* c, ast_t* ast)
{
  ast_t* id = ast_child(ast);
  const char* name = ast_name(id);

  LLVMValueRef value = codegen_getlocal(c, name);
  pony_assert(value != NULL);

  return value;
}

LLVMValueRef gen_localload(compile_t* c, ast_t* ast)
{
  LLVMValueRef local_ptr = gen_localptr(c, ast);

  if(local_ptr == NULL)
    return NULL;

  return LLVMBuildLoad(c->builder, local_ptr, "");
}

LLVMValueRef gen_addressof(compile_t* c, ast_t* ast)
{
  ast_t* expr = ast_child(ast);

  switch(ast_id(expr))
  {
    case TK_VARREF:
      return gen_localptr(c, expr);

    case TK_FVARREF:
      return gen_fieldptr(c, expr);

    case TK_FUNREF:
    case TK_BEREF:
      return gen_funptr(c, expr);

    default: {}
  }

  pony_assert(0);
  return NULL;
}

static LLVMValueRef gen_digestof_box(compile_t* c, reach_type_t* type,
  LLVMValueRef value, int boxed_subtype)
{
  pony_assert(LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMPointerTypeKind);

  LLVMBasicBlockRef box_block = NULL;
  LLVMBasicBlockRef nonbox_block = NULL;
  LLVMBasicBlockRef post_block = NULL;

  LLVMValueRef desc = gendesc_fetch(c, value);

  if((boxed_subtype & SUBTYPE_KIND_UNBOXED) != 0)
  {
    box_block = codegen_block(c, "digestof_box");
    nonbox_block = codegen_block(c, "digestof_nonbox");
    post_block = codegen_block(c, "digestof_post");

    // Check if it's a boxed value.
    LLVMValueRef type_id = gendesc_typeid(c, desc);
    LLVMValueRef boxed_mask = LLVMConstInt(c->i32, 1, false);
    LLVMValueRef is_boxed = LLVMBuildAnd(c->builder, type_id, boxed_mask, "");
    LLVMValueRef zero = LLVMConstInt(c->i32, 0, false);
    is_boxed = LLVMBuildICmp(c->builder, LLVMIntEQ, is_boxed, zero, "");
    LLVMBuildCondBr(c->builder, is_boxed, box_block, nonbox_block);
    LLVMPositionBuilderAtEnd(c->builder, box_block);
  }

  // Call the type-specific __digestof function, which will unbox the value.
  reach_method_t* digest_fn = reach_method(type, TK_BOX,
    stringtab("__digestof"), NULL);
  pony_assert(digest_fn != NULL);
  LLVMValueRef func = gendesc_vtable(c, desc, digest_fn->vtable_index);
  LLVMTypeRef fn_type = LLVMFunctionType(c->i64, &c->object_ptr, 1, false);
  func = LLVMBuildBitCast(c->builder, func, LLVMPointerType(fn_type, 0), "");
  LLVMValueRef box_digest = codegen_call(c, func, &value, 1, true);

  if((boxed_subtype & SUBTYPE_KIND_UNBOXED) != 0)
  {
    LLVMBuildBr(c->builder, post_block);

    // Just cast the address.
    LLVMPositionBuilderAtEnd(c->builder, nonbox_block);
    LLVMValueRef nonbox_digest = LLVMBuildPtrToInt(c->builder, value, c->i64,
      "");
    LLVMBuildBr(c->builder, post_block);

    LLVMPositionBuilderAtEnd(c->builder, post_block);
    LLVMValueRef phi = LLVMBuildPhi(c->builder, c->i64, "");
    LLVMAddIncoming(phi, &box_digest, &box_block, 1);
    LLVMAddIncoming(phi, &nonbox_digest, &nonbox_block, 1);
    return phi;
  } else {
    return box_digest;
  }
}

static LLVMValueRef gen_digestof_value(compile_t* c, ast_t* type,
  LLVMValueRef value)
{
  LLVMTypeRef impl_type = LLVMTypeOf(value);

  switch(LLVMGetTypeKind(impl_type))
  {
    case LLVMFloatTypeKind:
      value = LLVMBuildBitCast(c->builder, value, c->i32, "");
      return LLVMBuildZExt(c->builder, value, c->i64, "");

    case LLVMDoubleTypeKind:
      return LLVMBuildBitCast(c->builder, value, c->i64, "");

    case LLVMIntegerTypeKind:
    {
      uint32_t width = LLVMGetIntTypeWidth(impl_type);

      if(width < 64)
      {
        value = LLVMBuildZExt(c->builder, value, c->i64, "");
      } else if(width == 128) {
        LLVMValueRef shift = LLVMConstInt(c->i128, 64, false);
        LLVMValueRef high = LLVMBuildLShr(c->builder, value, shift, "");
        high = LLVMBuildTrunc(c->builder, high, c->i64, "");
        value = LLVMBuildTrunc(c->builder, value, c->i64, "");
        value = LLVMBuildXor(c->builder, value, high, "");
      }

      return value;
    }

    case LLVMStructTypeKind:
    {
      uint32_t count = LLVMCountStructElementTypes(impl_type);
      LLVMValueRef result = LLVMConstInt(c->i64, 0, false);
      ast_t* child = ast_child(type);

      for(uint32_t i = 0; i < count; i++)
      {
        LLVMValueRef elem = LLVMBuildExtractValue(c->builder, value, i, "");
        elem = gen_digestof_value(c, child, elem);
        result = LLVMBuildXor(c->builder, result, elem, "");
        child = ast_sibling(child);
      }

      pony_assert(child == NULL);

      return result;
    }

    case LLVMPointerTypeKind:
      if(!is_known(type))
      {
        reach_type_t* t = reach_type(c->reach, type);
        int sub_kind = subtype_kind(t);

        if((sub_kind & SUBTYPE_KIND_BOXED) != 0)
          return gen_digestof_box(c, t, value, sub_kind);
      }

      return LLVMBuildPtrToInt(c->builder, value, c->i64, "");

    default: {}
  }

  pony_assert(0);
  return NULL;
}

LLVMValueRef gen_digestof(compile_t* c, ast_t* ast)
{
  ast_t* expr = ast_child(ast);
  LLVMValueRef value = gen_expr(c, expr);
  return gen_digestof_value(c, ast_type(expr), value);
}

void gen_digestof_fun(compile_t* c, reach_type_t* t)
{
  pony_assert(t->can_be_boxed);

  reach_method_t* m = reach_method(t, TK_BOX, stringtab("__digestof"), NULL);

  if(m == NULL)
    return;

  compile_type_t* c_t = (compile_type_t*)t->c_type;
  compile_method_t* c_m = (compile_method_t*)m->c_method;
  c_m->func_type = LLVMFunctionType(c->i64, &c_t->structure_ptr, 1, false);
  c_m->func = codegen_addfun(c, m->full_name, c_m->func_type);

  codegen_startfun(c, c_m->func, NULL, NULL, false);
  LLVMValueRef value = LLVMGetParam(codegen_fun(c), 0);

  value = gen_unbox(c, t->ast_cap, value);
  LLVMBuildRet(c->builder, gen_digestof_value(c, t->ast_cap, value));

  codegen_finishfun(c);
}

LLVMValueRef gen_int(compile_t* c, ast_t* ast)
{
  ast_t* type = ast_type(ast);
  reach_type_t* t = reach_type(c->reach, type);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  lexint_t* value = ast_int(ast);
  LLVMValueRef vlow = LLVMConstInt(c->i128, value->low, false);
  LLVMValueRef vhigh = LLVMConstInt(c->i128, value->high, false);
  LLVMValueRef shift = LLVMConstInt(c->i128, 64, false);
  vhigh = LLVMConstShl(vhigh, shift);
  vhigh = LLVMConstAdd(vhigh, vlow);

  if(c_t->primitive == c->i128)
    return vhigh;

  if((c_t->primitive == c->f32) || (c_t->primitive == c->f64))
    return LLVMConstUIToFP(vhigh, c_t->primitive);

  return LLVMConstTrunc(vhigh, c_t->primitive);
}

LLVMValueRef gen_float(compile_t* c, ast_t* ast)
{
  ast_t* type = ast_type(ast);
  reach_type_t* t = reach_type(c->reach, type);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  return LLVMConstReal(c_t->primitive, ast_float(ast));
}

LLVMValueRef gen_string(compile_t* c, ast_t* ast)
{
  ast_t* type = ast_type(ast);
  const char* name = ast_name(ast);

  genned_string_t k;
  k.string = name;
  size_t index = HASHMAP_UNKNOWN;
  genned_string_t* string = genned_strings_get(&c->strings, &k, &index);

  if(string != NULL)
    return string->global;

  size_t len = ast_name_len(ast);

  LLVMValueRef args[4];
  args[0] = LLVMConstInt(c->i32, 0, false);
  args[1] = LLVMConstInt(c->i32, 0, false);

  LLVMValueRef str = LLVMConstStringInContext(c->context, name, (int)len,
    false);
  LLVMValueRef g_str = LLVMAddGlobal(c->module, LLVMTypeOf(str), "");
  LLVMSetLinkage(g_str, LLVMPrivateLinkage);
  LLVMSetInitializer(g_str, str);
  LLVMSetGlobalConstant(g_str, true);
  LLVMSetUnnamedAddr(g_str, true);
  LLVMValueRef str_ptr = LLVMConstInBoundsGEP(g_str, args, 2);

  reach_type_t* t = reach_type(c->reach, type);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  args[0] = c_t->desc;
  args[1] = LLVMConstInt(c->intptr, len, false);
  args[2] = LLVMConstInt(c->intptr, len + 1, false);
  args[3] = str_ptr;

  LLVMValueRef inst = LLVMConstNamedStruct(c_t->structure, args, 4);
  LLVMValueRef g_inst = LLVMAddGlobal(c->module, c_t->structure, "");
  LLVMSetInitializer(g_inst, inst);
  LLVMSetGlobalConstant(g_inst, true);
  LLVMSetLinkage(g_inst, LLVMPrivateLinkage);
  LLVMSetUnnamedAddr(g_inst, true);

  string = POOL_ALLOC(genned_string_t);
  string->string = name;
  string->global = g_inst;
  genned_strings_putindex(&c->strings, string, index);

  return g_inst;
}
