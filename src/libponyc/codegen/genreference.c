#include "genreference.h"
#include "genbox.h"
#include "gencall.h"
#include "gendesc.h"
#include "genexpr.h"
#include "genfun.h"
#include "genname.h"
#include "gentagged.h"
#include "genopt.h"
#include "gentype.h"
#include "../expr/literal.h"
#include "../reach/subtype.h"
#include "../type/cap.h"
#include "../type/subtype.h"
#include "../type/typealias.h"
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
  genned_string_hash, genned_string_cmp, genned_string_free);

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

  ast_t* def;
  ast_t* field;
  uint32_t index;
  get_fieldinfo(l_type, right, &def, &field, &index);

  if(ast_id(def) != TK_STRUCT)
    index++;

  if(ast_id(def) == TK_ACTOR)
    index++;

  reach_type_t* l_t = reach_type(c->reach, l_type, c->opt);
  pony_assert(l_t != NULL);
  compile_type_t* l_c_t = (compile_type_t*)l_t->c_type;

  return LLVMBuildStructGEP2(c->builder, l_c_t->structure, l_value, index, "");
}

LLVMValueRef gen_fieldptr(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, left, right);

  LLVMValueRef l_value = gen_expr(c, left);

  if(l_value == NULL)
    return NULL;

  ast_t* l_type = deferred_reify(c->frame->reify, ast_type(left), c->opt);

  // deferred_reify reifies typeparams against typeargs but doesn't
  // unfold typealiasrefs. For an alias-typed receiver (whether a
  // direct alias like `AliasInner is Inner` or a chain), l_type
  // arrives as TK_TYPEALIASREF and make_fieldptr's TK_NOMINAL
  // assertion fires. Unfold to the concrete head before passing in.
  ast_t* unfolded = NULL;
  if(ast_id(l_type) == TK_TYPEALIASREF)
  {
    unfolded = typealias_unfold(l_type);
    if(unfolded != NULL)
    {
      ast_free_unattached(l_type);
      l_type = unfolded;
    }
  }

  LLVMValueRef ret = make_fieldptr(c, l_value, l_type, right);
  ast_free_unattached(l_type);
  return ret;
}

LLVMValueRef gen_fieldload(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, left, right);

  LLVMValueRef field = gen_fieldptr(c, ast);

  if(field == NULL)
    return NULL;

  deferred_reification_t* reify = c->frame->reify;

  ast_t* type = deferred_reify(reify, ast_type(right), c->opt);
  reach_type_t* t = reach_type(c->reach, type, c->opt);
  pony_assert(t != NULL);
  ast_free_unattached(type);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  field = LLVMBuildLoad2(c->builder, c_t->mem_type, field, "");

  return gen_assign_cast(c, c_t->use_type, field, t->ast_cap);
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
  (void)l_type;
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

  deferred_reification_t* reify = c->frame->reify;

  ast_t* type = deferred_reify(reify, ast_type(ast), c->opt);
  reach_type_t* t = reach_type(c->reach, type, c->opt);
  pony_assert(t != NULL);
  ast_free_unattached(type);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  ast_t* l_type = deferred_reify(reify, ast_type(left), c->opt);
  LLVMValueRef value = make_tupleelemptr(c, l_value, l_type, right);
  ast_free_unattached(l_type);
  return gen_assign_cast(c, c_t->use_type, value, t->ast_cap);
}

LLVMValueRef gen_tuple(compile_t* c, ast_t* ast)
{
  ast_t* child = ast_child(ast);

  if(ast_sibling(child) == NULL)
    return gen_expr(c, child);

  deferred_reification_t* reify = c->frame->reify;

  ast_t* type = deferred_reify(reify, ast_type(ast), c->opt);

  // If we contain '_', we have no usable value.
  if(contains_dontcare(type))
  {
    ast_free_unattached(type);
    return GEN_NOTNEEDED;
  }

  reach_type_t* t = reach_type(c->reach, type, c->opt);
  ast_free_unattached(type);
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  int count = LLVMCountStructElementTypes(c_t->primitive);
  size_t buf_size = count * sizeof(LLVMTypeRef);
  LLVMTypeRef* elements = (LLVMTypeRef*)ponyint_pool_alloc_size(buf_size);
  LLVMGetStructElementTypes(c_t->primitive, elements);

  LLVMValueRef tuple = LLVMGetUndef(c_t->primitive);
  int i = 0;

  while(child != NULL)
  {
    LLVMValueRef value = gen_expr(c, child);

    if(value == NULL)
    {
      ponyint_pool_free_size(buf_size, elements);
      return NULL;
    }

    // We'll have an undefined element if one of our source elements is a
    // variable declaration. This is ok, since the tuple value will never be
    // used.
    if(value == GEN_NOVALUE || value == GEN_NOTNEEDED)
    {
      ponyint_pool_free_size(buf_size, elements);
      return value;
    }

    ast_t* child_type = deferred_reify(reify, ast_type(child), c->opt);
    value = gen_assign_cast(c, elements[i], value, child_type);
    ast_free_unattached(child_type);
    tuple = LLVMBuildInsertValue(c->builder, tuple, value, i++, "");
    child = ast_sibling(child);
  }

  ponyint_pool_free_size(buf_size, elements);
  return tuple;
}

LLVMValueRef gen_localdecl(compile_t* c, ast_t* ast)
{
  ast_t* id = ast_child(ast);
  const char* name = ast_name(id);

  // If this local has already been generated, don't create another copy. This
  // can happen when the same ast node is generated more than once, such as
  // the condition block of a while expression.
  LLVMValueRef value = codegen_getlocal(c, name);

  if(value != NULL)
    return GEN_NOVALUE;

  ast_t* type = deferred_reify(c->frame->reify, ast_type(id), c->opt);
  reach_type_t* t = reach_type(c->reach, type, c->opt);
  ast_free_unattached(type);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  // All alloca should happen in the entry block of a function.
  LLVMBasicBlockRef this_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef entry_block = LLVMGetEntryBasicBlock(codegen_fun(c));
  LLVMValueRef inst = LLVMGetFirstInstruction(entry_block);

  if(inst != NULL)
    LLVMPositionBuilderBefore(c->builder, inst);
  else
    LLVMPositionBuilderAtEnd(c->builder, entry_block);

  LLVMValueRef alloc = LLVMBuildAlloca(c->builder, c_t->mem_type, name);

  // Store the alloca to use when we reference this local.
  codegen_setlocal(c, name, alloc);

  LLVMMetadataRef file = codegen_difile(c);
  LLVMMetadataRef scope = codegen_discope(c);

  uint32_t align_bytes = LLVMABIAlignmentOfType(c->target_data, c_t->mem_type);

  LLVMMetadataRef info = LLVMDIBuilderCreateAutoVariable(c->di, scope,
    name, strlen(name), file, (unsigned)ast_line(ast), c_t->di_type,
    true, LLVMDIFlagZero, align_bytes * 8);

  LLVMMetadataRef expr = LLVMDIBuilderCreateExpression(c->di, NULL, 0);

  LLVMDIBuilderInsertDeclare(c->di, alloc, info, expr,
    (unsigned)ast_line(ast), (unsigned)ast_pos(ast), scope,
    LLVMGetInsertBlock(c->builder));

  // Put the builder back where it was.
  LLVMPositionBuilderAtEnd(c->builder, this_block);
  return GEN_NOTNEEDED;
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

  ast_t* type = deferred_reify(c->frame->reify, ast_type(ast), c->opt);
  reach_type_t* t = reach_type(c->reach, type, c->opt);
  ast_free_unattached(type);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  LLVMValueRef value = LLVMBuildLoad2(c->builder,
    LLVMGetAllocatedType(local_ptr), local_ptr, "");
  return gen_assign_cast(c, c_t->use_type, value, t->ast_cap);
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

  LLVMValueRef desc;

  desc = gentagged_fetch_desc_or_heap(c, value, type, "dg");

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
    stringtab(c->opt->strtab, "__digestof"), NULL, c->opt);
  pony_assert(digest_fn != NULL);
  LLVMValueRef func = gendesc_vtable(c, desc, digest_fn->vtable_index);
  LLVMTypeRef fn_type = LLVMFunctionType(c->intptr, &c->ptr, 1, false);
  LLVMValueRef box_digest = codegen_call(c, fn_type, func, &value, 1, true);

  if((boxed_subtype & SUBTYPE_KIND_UNBOXED) != 0)
  {
    LLVMBuildBr(c->builder, post_block);

    // Just cast the address.
    LLVMPositionBuilderAtEnd(c->builder, nonbox_block);
    LLVMValueRef nonbox_digest = LLVMBuildPtrToInt(c->builder, value, c->intptr,
      "");
    LLVMBuildBr(c->builder, post_block);

    LLVMPositionBuilderAtEnd(c->builder, post_block);
    LLVMValueRef phi = LLVMBuildPhi(c->builder, c->intptr, "");
    LLVMAddIncoming(phi, &box_digest, &box_block, 1);
    LLVMAddIncoming(phi, &nonbox_digest, &nonbox_block, 1);
    return phi;
  } else {
    return box_digest;
  }
}

static LLVMValueRef gen_digestof_int64(compile_t* c, LLVMValueRef value)
{
  pony_assert(LLVMTypeOf(value) == c->i64);

  if(target_is_ilp32(c->opt->triple))
  {
    LLVMValueRef shift = LLVMConstInt(c->i64, 32, false);
    LLVMValueRef high = LLVMBuildLShr(c->builder, value, shift, "");
    high = LLVMBuildTrunc(c->builder, high, c->i32, "");
    value = LLVMBuildTrunc(c->builder, value, c->i32, "");
    value = LLVMBuildXor(c->builder, value, high, "");
  }

  return value;
}

static LLVMValueRef gen_digestof_value(compile_t* c, ast_t* type,
  LLVMValueRef value)
{
  LLVMTypeRef impl_type = LLVMTypeOf(value);

  switch(LLVMGetTypeKind(impl_type))
  {
    case LLVMFloatTypeKind:
      value = LLVMBuildBitCast(c->builder, value, c->i32, "");
      return LLVMBuildZExt(c->builder, value, c->intptr, "");

    case LLVMDoubleTypeKind:
      value = LLVMBuildBitCast(c->builder, value, c->i64, "");
      return gen_digestof_int64(c, value);

    case LLVMIntegerTypeKind:
    {
      uint32_t width = LLVMGetIntTypeWidth(impl_type);

      if(width < 64)
      {
        return LLVMBuildZExt(c->builder, value, c->intptr, "");
      } else if(width == 64) {
        return gen_digestof_int64(c, value);
      } else if(width == 128) {
        LLVMValueRef shift = LLVMConstInt(c->i128, 64, false);
        LLVMValueRef high = LLVMBuildLShr(c->builder, value, shift, "");
        high = LLVMBuildTrunc(c->builder, high, c->i64, "");
        value = LLVMBuildTrunc(c->builder, value, c->i64, "");
        high = gen_digestof_int64(c, high);
        value = gen_digestof_int64(c, value);
        return LLVMBuildXor(c->builder, value, high, "");
      }
      break;
    }

    case LLVMStructTypeKind:
    {
      // Unfold type aliases to get the actual tuple element types.
      ast_t* iter_type = type;
      ast_t* unfolded = NULL;

      if(ast_id(type) == TK_TYPEALIASREF)
      {
        unfolded = typealias_unfold(type);

        if(unfolded != NULL)
          iter_type = unfolded;
      }

      uint32_t count = LLVMCountStructElementTypes(impl_type);
      LLVMValueRef result = LLVMConstInt(c->intptr, 0, false);
      ast_t* child = ast_child(iter_type);

      for(uint32_t i = 0; i < count; i++)
      {
        LLVMValueRef elem = LLVMBuildExtractValue(c->builder, value, i, "");
        elem = gen_digestof_value(c, child, elem);
        result = LLVMBuildXor(c->builder, result, elem, "");
        child = ast_sibling(child);
      }

      pony_assert(child == NULL);

      if(unfolded != NULL)
        ast_free_unattached(unfolded);

      return result;
    }

    case LLVMPointerTypeKind:
      if(!is_known(type))
      {
        reach_type_t* t = reach_type(c->reach, type, c->opt);
        int sub_kind = subtype_kind(t);

        if((sub_kind & SUBTYPE_KIND_BOXED) != 0)
          return gen_digestof_box(c, t, value, sub_kind);
      }

      return LLVMBuildPtrToInt(c->builder, value, c->intptr, "");

    default: {}
  }

  pony_assert(0);
  return NULL;
}

LLVMValueRef gen_digestof(compile_t* c, ast_t* ast)
{
  ast_t* expr = ast_child(ast);
  LLVMValueRef value = gen_expr(c, expr);
  ast_t* type = deferred_reify(c->frame->reify, ast_type(expr), c->opt);
  LLVMValueRef ret = gen_digestof_value(c, type, value);
  ast_free_unattached(type);
  return ret;
}

void gen_digestof_fun(compile_t* c, reach_type_t* t)
{
  pony_assert(t->can_be_boxed);

  reach_method_t* m = reach_method(t, TK_BOX, stringtab(c->opt->strtab, "__digestof"), NULL, c->opt);

  if(m == NULL)
    return;

  compile_type_t* c_t = (compile_type_t*)t->c_type;
  compile_method_t* c_m = (compile_method_t*)m->c_method;
  c_m->func_type = LLVMFunctionType(c->intptr, &c_t->structure_ptr, 1, false);
  c_m->func = codegen_addfun(c, m->full_name, c_m->func_type, true);

  codegen_startfun(c, c_m->func, NULL, NULL, NULL, false);
  LLVMValueRef value = LLVMGetParam(codegen_fun(c), 0);

  value = gen_unbox(c, t->ast_cap, value);
  genfun_build_ret(c, gen_digestof_value(c, t->ast_cap, value));

  codegen_finishfun(c);
}

LLVMValueRef gen_int(compile_t* c, ast_t* ast)
{
  ast_t* type = deferred_reify(c->frame->reify, ast_type(ast), c->opt);
  reach_type_t* t = reach_type(c->reach, type, c->opt);
  ast_free_unattached(type);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  lexint_t* value = ast_int(ast);
  LLVMValueRef vlow = LLVMConstInt(c->i128, value->low, false);
  LLVMValueRef vhigh = LLVMConstInt(c->i128, value->high, false);
  LLVMValueRef shift = LLVMConstInt(c->i128, 64, false);
  vhigh = LLVMBuildShl(c->builder, vhigh, shift, "");
  vhigh = LLVMBuildAdd(c->builder, vhigh, vlow, "");

  if(c_t->primitive == c->i128)
    return vhigh;

  if((c_t->primitive == c->f32) || (c_t->primitive == c->f64))
    return LLVMBuildUIToFP(c->builder, vhigh, c_t->primitive, "");

  return LLVMConstTrunc(vhigh, c_t->primitive);
}

LLVMValueRef gen_float(compile_t* c, ast_t* ast)
{
  ast_t* type = deferred_reify(c->frame->reify, ast_type(ast), c->opt);
  reach_type_t* t = reach_type(c->reach, type, c->opt);
  ast_free_unattached(type);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  return LLVMConstReal(c_t->primitive, ast_float(ast));
}

LLVMValueRef gen_array_const(compile_t* c, ast_t* recover)
{
  ast_t* body = ast_childidx(recover, 1);
  ast_t* seq = ast_child(body);

  // Get the Array[T] type from the recover node (which has cap val).
  deferred_reification_t* reify = c->frame->reify;
  ast_t* array_type = deferred_reify(reify, ast_type(recover), c->opt);

  reach_type_t* t = reach_type(c->reach, array_type, c->opt);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  // Extract the element type T from Array[T].
  ast_t* typeargs = ast_childidx(array_type, 2);
  ast_t* elem_type = ast_child(typeargs);
  reach_type_t* elem_t = reach_type(c->reach, elem_type, c->opt);
  compile_type_t* elem_c_t = (compile_type_t*)elem_t->c_type;
  LLVMTypeRef elem_llvm_type = elem_c_t->primitive;

  // Count elements: every TK_CALL child of the TK_SEQ after the first
  // child (the TK_ASSIGN) and before the last (the TK_REFERENCE) is a
  // push() call.
  size_t count = 0;
  ast_t* child = ast_sibling(ast_child(seq));
  while(child != NULL && ast_id(child) == TK_CALL)
  {
    count++;
    child = ast_sibling(child);
  }

  LLVMValueRef* elements =
    (LLVMValueRef*)ponyint_pool_alloc_size(count * sizeof(LLVMValueRef));

  size_t i = 0;
  child = ast_sibling(ast_child(seq));
  while(child != NULL && ast_id(child) == TK_CALL)
  {
    // TK_CALL → child(1) TK_POSITIONALARGS → child(0) TK_SEQ → child(0)
    ast_t* pos_args = ast_childidx(child, 1);
    ast_t* arg_seq = ast_child(pos_args);
    ast_t* lit = ast_child(arg_seq);

    switch(ast_id(lit))
    {
      case TK_INT:
      {
        lexint_t* value = ast_int(lit);
        if((elem_llvm_type == c->f32) || (elem_llvm_type == c->f64))
        {
          double d = (double)value->high * 18446744073709551616.0
            + (double)value->low;
          elements[i] = LLVMConstReal(elem_llvm_type, d);
        }
        else if(elem_llvm_type == c->i128)
        {
          uint64_t words[2] = { value->low, value->high };
          elements[i] = LLVMConstIntOfArbitraryPrecision(c->i128, 2, words);
        }
        else
        {
          elements[i] = LLVMConstInt(elem_llvm_type, value->low, false);
        }
        break;
      }

      case TK_FLOAT:
        elements[i] = LLVMConstReal(elem_llvm_type, ast_float(lit));
        break;

      case TK_TRUE:
        elements[i] = LLVMConstInt(elem_llvm_type, 1, false);
        break;

      case TK_FALSE:
        elements[i] = LLVMConstInt(elem_llvm_type, 0, false);
        break;

      default:
        pony_assert(0);
        break;
    }
    i++;
    child = ast_sibling(child);
  }

  pony_assert(i == count);

  LLVMValueRef const_data =
    LLVMConstArray(elem_llvm_type, elements, (unsigned)count);
  LLVMTypeRef data_type = LLVMArrayType(elem_llvm_type, (unsigned)count);
  LLVMValueRef g_data = LLVMAddGlobal(c->module, data_type, "");
  LLVMSetLinkage(g_data, LLVMPrivateLinkage);
  LLVMSetInitializer(g_data, const_data);
  LLVMSetGlobalConstant(g_data, true);
  LLVMSetUnnamedAddr(g_data, true);

  ponyint_pool_free_size(count * sizeof(LLVMValueRef), elements);

  LLVMValueRef gep_indices[2];
  gep_indices[0] = LLVMConstInt(c->i32, 0, false);
  gep_indices[1] = LLVMConstInt(c->i32, 0, false);
  LLVMValueRef data_ptr =
    LLVMConstInBoundsGEP2(data_type, g_data, gep_indices, 2);

  // Build the Array struct: {descriptor, _size, _alloc, _ptr}.
  LLVMValueRef args[4];
  args[0] = codegen_resolve_global(c, c_t->desc);
  args[1] = LLVMConstInt(c->intptr, count, false);
  args[2] = LLVMConstInt(c->intptr, count, false);
  args[3] = data_ptr;

  LLVMValueRef inst = LLVMConstNamedStruct(c_t->structure, args, 4);
  LLVMValueRef g_inst = LLVMAddGlobal(c->module, c_t->structure, "");
  LLVMSetInitializer(g_inst, inst);
  LLVMSetGlobalConstant(g_inst, true);
  LLVMSetLinkage(g_inst, LLVMPrivateLinkage);
  LLVMSetUnnamedAddr(g_inst, true);

  ast_free_unattached(array_type);
  return g_inst;
}

LLVMValueRef gen_string(compile_t* c, ast_t* ast)
{
  const char* name = ast_name(ast);

  genned_string_t k;
  k.string = name;
  size_t index = HASHMAP_UNKNOWN;
  genned_string_t* string = genned_strings_get(&c->strings, &k, &index);

  if(string != NULL)
    return string->global;

  ast_t* type = ast_type(ast);
  pony_assert(is_literal(type, "String"));
  reach_type_t* t = reach_type(c->reach, type, c->opt);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  size_t len = ast_name_len(ast);

  LLVMValueRef args[4];
  args[0] = codegen_resolve_global(c, c_t->desc);
  args[1] = LLVMConstInt(c->intptr, len, false);
  args[2] = LLVMConstInt(c->intptr, len + 1, false);
  args[3] = codegen_string(c, name, len);

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
