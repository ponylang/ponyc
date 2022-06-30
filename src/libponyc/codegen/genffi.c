#include "genffi.h"
#include "gencall.h"
#include "gentype.h"
#include "genexpr.h"
#include "../type/subtype.h"
#include "../../libponyrt/mem/pool.h"
#include "../../libponyrt/mem/heap.h"
#include "ponyassert.h"
#include <string.h>

struct ffi_decl_t
{
  LLVMValueRef func;

  // We store and lookup the first declaration encountered.
  ast_t* decl;

  // We store the first call encountered in the central hash map, but in our
  // local copy after lookup, we keep using the current call being processed.
  ast_t* call;
};

static size_t ffi_decl_hash(ffi_decl_t* d)
{
  return ponyint_hash_ptr(d->func);
}

static bool ffi_decl_cmp(ffi_decl_t* a, ffi_decl_t* b)
{
  return a->func == b->func;
}

static void ffi_decl_free(ffi_decl_t* d)
{
  POOL_FREE(ffi_decl_t, d);
}

DEFINE_HASHMAP(ffi_decls, ffi_decls_t, ffi_decl_t, ffi_decl_hash, ffi_decl_cmp,
  ffi_decl_free);

static const char* ffi_decl_name(ffi_decl_t* ffi_decl)
{
  // Use either the first declaration (if we have one) or the first call site
  // to get the name of the FFI function being called.
  ast_t* ast = ffi_decl->decl;
  if (ast == NULL)
    ast = ffi_decl->call;
  pony_assert(ast != NULL);

  // Note that for both declaration and call ASTs, the name is in the 1st child.
  ast_t* id = ast_child(ast);

  // Get the function name, +1 to skip leading @
  return ast_name(id) + 1;
}

static bool ffi_decl_can_error(ffi_decl_t* ffi_decl)
{
  // Use either the first declaration (if we have one) or the first call site
  // to get the name of the FFI function being called.
  ast_t* ast = ffi_decl->decl;
  if (ast == NULL)
    ast = ffi_decl->call;
  pony_assert(ast != NULL);

  // Note that for both declaration and call ASTs, the AST position where the
  // question mark would be marked (if present) is the last child.
  return ast_id(ast_childlast(ast)) == TK_QUESTION;
}

static bool ffi_decl_is_var_args(ffi_decl_t* ffi_decl)
{
  pony_assert(ffi_decl->decl != NULL);

  // An FFI declaration is considered as being a var-args function
  // if it is marked with a final pseudo-argument of an ellipsis (`...`).
  ast_t* ast_args = ast_childidx(ffi_decl->decl, 2);
  ast_t* last_arg = ast_childlast(ast_args);
  return (last_arg != NULL) && (ast_id(last_arg) == TK_ELLIPSIS);
}

static bool ffi_decl_is_intrinsic(ffi_decl_t* ffi_decl)
{
  // The function is intrinsic if its name begins with one of these prefixes.
  const char* name = ffi_decl_name(ffi_decl);
  return !strncmp(name, "llvm.", 5) || !strncmp(name, "internal.", 9);
}

static reach_type_t* ffi_decl_reach_type_for_return(compile_t* c,
  ffi_decl_t* ffi_decl)
{
  pony_assert(ffi_decl->call != NULL);

  // Get the reach_type_t for the return value of this FFI call site.
  // Note that this can in some cases vary compared to that of the decl site.
  deferred_reification_t* reify = c->frame->reify;
  ast_t* ast = deferred_reify(reify, ast_type(ffi_decl->call), c->opt);
  reach_type_t* t = reach_type(c->reach, ast);
  ast_free_unattached(ast);

  pony_assert(t != NULL);
  return t;
}

static bool is_ffi_llvm_type_indirect(compile_t* c, LLVMTypeRef t)
{
  // LLVM struct types are passed indirectly in cdecl ABI when their
  // type exceeds a certain size, which is specific to a platform.
  // It happens to be the same on all the platforms we currently support,
  // but for defensiveness, we will raise an error here if the platform
  // being compiled is not one of the known supported ones.
  const size_t size = (size_t)LLVMABISizeOfType(c->target_data, t);
  return size > // (in bytes)
#if defined(PLATFORM_IS_X86) || defined(PLATFORM_IS_ARM)
  16;
#else
#  error PLATFORM NOT SUPPORTED!
#endif
}

static bool is_ffi_type_indirect(compile_t* c, reach_type_t* t) {
  // Non-tuple types are always passed directly.
  if (t->underlying != TK_TUPLETYPE)
    return false;

  return is_ffi_llvm_type_indirect(c, ((compile_type_t*)t->c_type)->use_type);
}

static bool ffi_decl_has_indirect_return(compile_t* c, ffi_decl_t* ffi_decl)
{
  // An intrinsic function never has an indirect return value, because it is
  // not truly an external function being called via the cdecl ABI.
  if (ffi_decl_is_intrinsic(ffi_decl))
    return false;

  // Check if the reach type for the FFI value is one that needs indirection.
  // This answer can vary on a platform-specific basis.
  return is_ffi_type_indirect(c, ffi_decl_reach_type_for_return(c, ffi_decl));
}

static LLVMTypeRef ffi_decl_llvm_return_type(compile_t* c, ffi_decl_t* ffi_decl)
{
  reach_type_t* t = ffi_decl_reach_type_for_return(c, ffi_decl);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  // If this is an intrinsic return tuple, we want to use the `use_type` of
  // each element, rather than the `mem_type` of each element, which is what
  // is listed in the `use_type` for the struct representing the tuple.
  //
  // We don't do this for non-intrinsic return tuples because those ones are
  // being returned as true C structs, and hence need to use the `mem_type`s.
  if((t->underlying == TK_TUPLETYPE) && ffi_decl_is_intrinsic(ffi_decl))
  {
    // Can't use the named type. Build an unnamed type with the same elements.
    unsigned int elements_count = LLVMCountStructElementTypes(c_t->use_type);
    size_t elements_buf_size = elements_count * sizeof(LLVMTypeRef);
    LLVMTypeRef* elements =
      (LLVMTypeRef*)ponyint_pool_alloc_size(elements_buf_size);

    // Loop over each of the elements here and get the `use_type`.
    ast_t* child = ast_child(t->ast);
    for(size_t i = 0; child != NULL; i++)
    {
      compile_type_t* child_c_t = (compile_type_t*)t->fields[i].type->c_type;
      elements[i] = child_c_t->use_type;

      child = ast_sibling(child);
    }

    // Create the new anonymous LLVM struct type and return it.
    // After creating the LLVM type we are able to free the list we allocated.
    LLVMTypeRef anon_struct_type =
      LLVMStructTypeInContext(c->context, elements, elements_count, false);
    ponyint_pool_free_size(elements_buf_size, elements);
    return anon_struct_type;
  }

  // When the FFI return type is given as None, we interpret that as `void`.
  if(is_none(t->ast_cap))
    return c->void_type;

  // All other return types pass through this reinterpretation step unchanged.
  return c_t->use_type;
}

static LLVMTypeRef abi_return_type_for(compile_t* c, LLVMTypeRef return_type)
{
  // Only struct return values are handled specially by this ABI transformation.
  // All other types will pass through unchanged.
  if(LLVMGetTypeKind(return_type) != LLVMStructTypeKind)
    return return_type;

  // We should never reach this point if the return type demands indirection.
  pony_assert(!is_ffi_llvm_type_indirect(c, return_type));

  // Determine the total struct size and the size of a register on this target.
  int total_size = (int)LLVMABISizeOfType(c->target_data, return_type);
  int register_size = (int)LLVMABISizeOfType(c->target_data, c->intptr);

  // We will split the total size into register-sized chunks, rounding
  // up to the next largest number of registers (i.e. ceiling division).
  // This gives us enough registers to fully cover the total size.
  int registers_count = (total_size + register_size - 1) / register_size;

  // Allocate a buffer to hold the list of LLVM registers we will collect.
  LLVMTypeRef* registers = NULL;
  if(registers_count > 0)
    registers = (LLVMTypeRef*)ponyint_pool_alloc_size(
      registers_count * sizeof(LLVMTypeRef));

  // Fill the buffer with register-sized integers.
  int registers_total_size = 0;
  for(int i = 0; i < registers_count; i++)
  {
    int remaining_size = total_size - registers_total_size;
    if(remaining_size >= register_size)
    {
      registers[i] = c->intptr;
      registers_total_size += register_size;
    } else if(remaining_size >= 8) {
      registers[i] = c->i64;
      registers_total_size += 8;
    } else if(remaining_size >= 4) {
      registers[i] = c->i32;
      registers_total_size += 4;
    } else if(remaining_size >= 2) {
      registers[i] = c->i16;
      registers_total_size += 2;
    } else if(remaining_size >= 1) {
      registers[i] = c->i8;
      registers_total_size += 1;
    } else {
      pony_assert(false);
    }
  }

  // Create an anonymous struct type with the collected registers as elements.
  LLVMTypeRef anon_struct_type =
    LLVMStructTypeInContext(c->context, registers, registers_count, false);

  // We no longer need the register type list that we allocated.
  if(registers != NULL)
    ponyint_pool_free_size(registers_count * sizeof(LLVMTypeRef), registers);

  // Return the anonymous struct type - this is our ABI-friendly return type.
  return anon_struct_type;
}

static void ffi_decl_gen_func(compile_t* c, ffi_decl_t* ffi_decl)
{
  pony_assert(ffi_decl->func == NULL);
  pony_assert(ffi_decl->decl != NULL);
  pony_assert(ffi_decl->call != NULL);

  // Get the return type of this function. We'll use it later in a few places.
  LLVMTypeRef llvm_return_type = ffi_decl_llvm_return_type(c, ffi_decl);

  // As a baseline, the number of actual parameters will match the number of
  // params in the AST declaration. We may adjust it in the next few steps.
  ast_t* ast_params = ast_childidx(ffi_decl->decl, 2);
  int ffi_params_count = (int)ast_childcount(ast_params);

  // Determine if this function needs to use an indirect return approach,
  // in which we will pass in a pointer to an alloca that will be filled
  // by the callee in lieu of a real return value.
  // If true, the number of function parameters is increased to account for it.
  bool has_indirect_return = ffi_decl_has_indirect_return(c, ffi_decl);
  if (has_indirect_return)
    ffi_params_count++;

  // Determine if this function is a var-args function.
  // If true, the number of parameters is decreased by one because the last
  // parameter in the declaration was an ellipsis (`...`) pseudo-parameter.
  // Thus the `ffi_params_count` variable reflects the number of required parameters,
  // while the variable/optional parameters following that are not in the count.
  bool is_var_args = ffi_decl_is_var_args(ffi_decl);
  if(is_var_args)
    ffi_params_count--;

  // Allocate a buffer to hold the list of LLVM parameter types we will collect.
  LLVMTypeRef* ffi_params = NULL;
  if(ffi_params_count > 0)
    ffi_params = (LLVMTypeRef*)ponyint_pool_alloc_size(
      ffi_params_count * sizeof(LLVMTypeRef));

  // Loop over the list of AST parameters and collect the LLVM parameter types.
  deferred_reification_t* reify = c->frame->reify;
  ast_t* ast_param = ast_child(ast_params);
  for(int i = 0; i < ffi_params_count; i++)
  {
    // If the return value is indirect, we prepend a hidden parameter which is
    // the destination address for it - this is how indirect returns work.
    if(has_indirect_return && (i == 0))
    {
      ffi_params[i] = LLVMPointerType(llvm_return_type, 0);
      continue; // go to the first real parameter, with `i == 1`
    }

    // Get the `reach_type_t` for this AST parameter.
    ast_t* ast_param_type = ast_type(ast_param);
    if(ast_param_type == NULL)
      ast_param_type = ast_childidx(ast_param, 1);
    ast_param_type = deferred_reify(reify, ast_param_type, c->opt);
    reach_type_t* param_type = reach_type(c->reach, ast_param_type);
    pony_assert(param_type != NULL);
    ast_free_unattached(ast_param_type);

    // Collect the LLVM type into the list.
    // If this parameter is marked as indirect, we add pointer indirection.
    LLVMTypeRef ffi_param_type =
      ((compile_type_t*)param_type->c_type)->use_type;
    ffi_params[i] = is_ffi_type_indirect(c, param_type)
      ? LLVMPointerType(ffi_param_type, 0)
      : ffi_param_type;

    // Go to the next AST parameter and continue the loop.
    ast_param = ast_sibling(ast_param);
  }

  // Determine the return type that should go in the actual function signature.
  // Note that if the return value is indirect, the return type will be void.
  // Otherwise it will be the specified return type, maybe coerced for the ABI.
  LLVMTypeRef sig_return_type =
    has_indirect_return ? c->void_type : abi_return_type_for(c, llvm_return_type);

  // Finally, declare the function with the given signature.
  LLVMTypeRef f_type = LLVMFunctionType(sig_return_type,
    ffi_params, ffi_params_count, is_var_args);
  LLVMValueRef func =
    LLVMAddFunction(c->module, ffi_decl_name(ffi_decl), f_type);

  // We no longer need the parameter list that we allocated.
  if(ffi_params != NULL)
    ponyint_pool_free_size(ffi_params_count * sizeof(LLVMTypeRef), ffi_params);

  // Emit the newly declared function by writing it into the `ffi_decl` struct.
  ffi_decl->func = func;
}

static LLVMValueRef make_ffi_indirect_alloca(compile_t* c,
  LLVMTypeRef value_type)
{
  // Note that an alloca should always be created in the entry block.
  // So we take note of the current block and move to the entry block.
  LLVMBasicBlockRef orig_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef entry_block = LLVMGetEntryBasicBlock(codegen_fun(c));
  LLVMValueRef first_inst = LLVMGetFirstInstruction(entry_block);
  if(first_inst != NULL)
    LLVMPositionBuilderBefore(c->builder, first_inst);
  else
    LLVMPositionBuilderAtEnd(c->builder, entry_block);

  // Create the alloca here in the entry block.
  LLVMValueRef alloca = LLVMBuildAlloca(c->builder, value_type, "ffi_indirect");

  // Move back to the original block.
  LLVMPositionBuilderAtEnd(c->builder, orig_block);

  return alloca;
}

static void report_ffi_type_err(compile_t* c, ffi_decl_t* ffi_decl,
  const char* name)
{
  pony_assert(ffi_decl->decl != NULL);
  pony_assert(ffi_decl->call != NULL);

  ast_error(c->opt->check.errors, ffi_decl->call,
    "conflicting calls for FFI function: %s have incompatible types",
    name);

  ast_error_continue(c->opt->check.errors, ffi_decl->decl,
    "first declaration is here");
}

static bool ffi_decl_get_or_gen(compile_t* c, ffi_decl_t* ffi_decl)
{
  pony_assert(ffi_decl->func == NULL);
  pony_assert(ffi_decl->decl == NULL);
  pony_assert(ffi_decl->call != NULL);

  // Get the function. First check if the name is in use by a global and error
  // if it's the case.
  bool is_func = false;
  ffi_decl->func = LLVMGetNamedGlobal(c->module, ffi_decl_name(ffi_decl));

  if(ffi_decl->func == NULL)
  {
    ffi_decl->func = LLVMGetNamedFunction(c->module, ffi_decl_name(ffi_decl));
    is_func = true;
  }

  if(ffi_decl->func == NULL)
  {
    // Prototypes are mandatory, the declaration is already stored.
    ffi_decl->decl = (ast_t*)ast_data(ffi_decl->call);
    pony_assert(ffi_decl->decl != NULL);

    ffi_decl_gen_func(c, ffi_decl);
    pony_assert(ffi_decl->func != NULL);

    size_t index = HASHMAP_UNKNOWN;

#ifndef PONY_NDEBUG
    ffi_decl_t k;
    k.func = ffi_decl->func;

    ffi_decl_t* existing_ffi_decl = ffi_decls_get(&c->ffi_decls, &k, &index);
    pony_assert(existing_ffi_decl == NULL);
#endif

    ffi_decl_t* new_ffi_decl = POOL_ALLOC(ffi_decl_t);
    new_ffi_decl->func = ffi_decl->func;
    new_ffi_decl->decl = ffi_decl->decl;
    new_ffi_decl->call = ffi_decl->call;

    ffi_decls_putindex(&c->ffi_decls, new_ffi_decl, index);
    return true;
  }

  ffi_decl_t k;
  k.func = ffi_decl->func;
  size_t index = HASHMAP_UNKNOWN;

  ffi_decl_t* existing_ffi_decl = ffi_decls_get(&c->ffi_decls, &k, &index);

  if((existing_ffi_decl == NULL) && (!is_func || LLVMHasMetadataStr(ffi_decl->func, "pony.abi")))
  {
    ast_error(c->opt->check.errors, ffi_decl->call, "cannot use '%s' "
      "as an FFI name: name is already in use by the internal ABI",
      ffi_decl_name(ffi_decl));
    return false;
  }

  ffi_decl->func = existing_ffi_decl->func;
  ffi_decl->decl = existing_ffi_decl->decl;
  // Do NOT copy the `existing_ffi_decl->call` into our `ffi_decl->call`.
  // We want to keep using our own call site AST in our local copy of this
  // data structure, because the types may differ at different call sites.

  pony_assert(is_func);

  return true;
}

static LLVMValueRef cast_abi_ffi_struct_value(compile_t* c, LLVMValueRef value,
  LLVMTypeRef dest_type)
{
  // LLVM does not allow directly casting a struct value to another struct value
  // so we need to do a silly trick here - but it's apparently what clang does,
  // and it can be seen in clang output that at O1 optimization level and higher
  // the LLVM passes will replace the copy with more optimized bit operations,
  // removing the unnecessary alloca load and store instructions.

  // Create a source and destination alloca of the respective types.
  LLVMTypeRef source_type = LLVMTypeOf(value);
  LLVMValueRef source = make_ffi_indirect_alloca(c, source_type);
  LLVMValueRef dest = make_ffi_indirect_alloca(c, dest_type);

  // Store the value into the source alloca.
  LLVMBuildStore(c->builder, value, source);

  // Cast both allocas to byte pointers and copy from source to dest.
  LLVMValueRef source_bytes = LLVMBuildBitCast(c->builder, source, c->void_ptr, "");
  LLVMValueRef dest_bytes = LLVMBuildBitCast(c->builder, dest, c->void_ptr, "");
  int bytes_count = (int)LLVMABISizeOfType(c->target_data, source_type);
  LLVMValueRef bytes_count_value = LLVMConstInt(c->intptr, bytes_count, false);
  gencall_memcpy(c, dest_bytes, source_bytes, bytes_count_value);

  // Load the casted value from the dest alloca.
  return LLVMBuildLoad_P(c->builder, dest, "");
}

static LLVMValueRef cast_indirect_ffi_arg(compile_t* c, ffi_decl_t* decl,
  LLVMValueRef arg, LLVMTypeRef param_type);

static LLVMValueRef cast_ffi_value(compile_t* c, ffi_decl_t* ffi_decl,
  LLVMValueRef arg, LLVMTypeRef param_type, bool is_result)
{
  if(arg == NULL)
    return NULL;

  LLVMTypeRef arg_type = LLVMTypeOf(arg);

  if(param_type == arg_type)
    return arg;

  if((ponyint_next_pow2(LLVMABISizeOfType(c->target_data, param_type)) !=
    ponyint_next_pow2(LLVMABISizeOfType(c->target_data, arg_type))))
  {
    report_ffi_type_err(c, ffi_decl, is_result ? "return values" : "parameters");
    return NULL;
  }

  switch(LLVMGetTypeKind(param_type))
  {
    case LLVMPointerTypeKind:
      if((LLVMGetTypeKind(arg_type) == LLVMStructTypeKind) && !is_result)
        return cast_indirect_ffi_arg(c, ffi_decl, arg, param_type);
      else if(LLVMGetTypeKind(arg_type) == LLVMIntegerTypeKind)
        return LLVMBuildIntToPtr(c->builder, arg, param_type, "");
      else
        return LLVMBuildBitCast(c->builder, arg, param_type, "");

    case LLVMIntegerTypeKind:
      if(LLVMGetTypeKind(arg_type) == LLVMPointerTypeKind)
        return LLVMBuildPtrToInt(c->builder, arg, param_type, "");
      break;

    case LLVMStructTypeKind:
      pony_assert(LLVMGetTypeKind(arg_type) == LLVMStructTypeKind);
      if((param_type != arg_type) && is_result)
        return cast_abi_ffi_struct_value(c, arg, param_type);

      return arg;

    default: {}
  }

  pony_assert(false);
  return NULL;
}

static LLVMValueRef cast_indirect_ffi_arg(compile_t* c, ffi_decl_t* decl,
  LLVMValueRef arg, LLVMTypeRef param_type)
{
  // The param is a struct pointer and the arg is a struct value, so we need
  // to create an alloca to use as the pointer and store the arg value there.
  // This is how the cdecl ABI does indirect arg passing to a function.
  LLVMTypeRef arg_type = LLVMTypeOf(arg);
  pony_assert(LLVMGetTypeKind(arg_type) == LLVMStructTypeKind);
  pony_assert(LLVMGetTypeKind(param_type) == LLVMPointerTypeKind);

  // First we need to cast the argument to the type pointed to by the parameter,
  // using the more permissive cast style we use for FFI values.
  LLVMTypeRef cast_type = LLVMGetElementType(param_type);
  LLVMValueRef cast_arg = cast_ffi_value(c, decl, arg, cast_type, false);

  // Create an alloca of the parameter type and store the cast argument in it.
  LLVMValueRef alloca = make_ffi_indirect_alloca(c, arg_type);
  LLVMBuildStore(c->builder, cast_arg, alloca);

  // Return the alloca, which the outer code will use as the FFI call argument.
  return alloca;
}

static LLVMValueRef* ffi_decl_alloc_and_fill_args_list(compile_t* c,
  ffi_decl_t* ffi_decl, int* out_count)
{
  // Determine how many LLVM-level arguments we will pass to the FFI function.
  // Note that this may be one more than the number of Pony-level arguments
  // if we are passing the return value indirectly.
  ast_t* ast_args = ast_childidx(ffi_decl->call, 2);
  int count = (int)ast_childcount(ast_args);
  bool has_indirect_return = ffi_decl_has_indirect_return(c, ffi_decl);
  if(has_indirect_return)
    count++;

  // Let the caller know the count as well.
  *out_count = count;

  // If there are zero arguments, we don't need to do anything further.
  if (count == 0)
    return NULL;

  // Allocate a buffer to hold the list of argument values.
  size_t buf_size = count * sizeof(LLVMValueRef);
  LLVMValueRef* ffi_args = (LLVMValueRef*)ponyint_pool_alloc_size(buf_size);

  // TODO: Add comments for everything below here, and validate that logic.

  LLVMTypeRef f_type = LLVMGetElementType(LLVMTypeOf(ffi_decl->func));
  bool vararg = (LLVMIsFunctionVarArg(f_type) != 0);

  LLVMTypeRef* f_params = NULL;
  if(!vararg)
  {
    if(count != (int)LLVMCountParamTypes(f_type))
    {
      ast_error(c->opt->check.errors, ffi_decl->call,
        "conflicting declarations for FFI function: declarations have an "
        "incompatible number of parameters");

      ast_error_continue(c->opt->check.errors, ffi_decl->decl,
        "first declaration is here");

      return NULL;
    }

    f_params = (LLVMTypeRef*)ponyint_pool_alloc_size(buf_size);
    LLVMGetParamTypes(f_type, f_params);
  }

  ast_t* arg = ast_child(ast_args);

  for(int i = 0; i < count; i++)
  {
    ffi_args[i] = gen_expr(c, arg);

    if(has_indirect_return && (i == 0))
    {
      ffi_args[i] = make_ffi_indirect_alloca(c, LLVMGetElementType(f_params[i]));
      continue;
    }

    if(!vararg)
      ffi_args[i] = cast_ffi_value(c, ffi_decl, ffi_args[i], f_params[i], false);

    if(ffi_args[i] == NULL)
    {
      ponyint_pool_free_size(buf_size, ffi_args);
      return NULL;
    }

    arg = ast_sibling(arg);
  }

  if(!vararg)
    ponyint_pool_free_size(buf_size, f_params);

  return ffi_args;
}

static LLVMValueRef ffi_decl_process_return_value(compile_t* c,
  ffi_decl_t* ffi_decl, LLVMValueRef result)
{
  reach_type_t* t = ffi_decl_reach_type_for_return(c, ffi_decl);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  // Special case a None return value, which is used for void functions.
  bool isnone = is_none(t->ast);
  bool isvoid = LLVMTypeOf(result) == c->void_type;
  if(isnone && isvoid)
  {
    result = c_t->instance;
  } else if(isnone != isvoid) {
    report_ffi_type_err(c, ffi_decl, "return values");
    return NULL;
  }

  // Do FFI-specific casting procedures, which are more permissive
  // (or more complicated from an ABI perspective) than normal casting we do.
  result = cast_ffi_value(c, ffi_decl, result, c_t->use_type, true);

  // TODO: Can this be removed given we now have the above casting?
  // If not, then we need to document here what situations this is needed for.
  result = gen_assign_cast(c, c_t->use_type, result, t->ast_cap);

  return result;
}

LLVMValueRef ffi_decl_gen_call(compile_t* c, ffi_decl_t* ffi_decl)
{
  // Generate the list of argument values to pass to the FFI function.
  int ffi_args_count;
  LLVMValueRef* ffi_args = ffi_decl_alloc_and_fill_args_list(c, ffi_decl,
    &ffi_args_count);

  // Set the debug info location to point to this call site AST.
  codegen_debugloc(c, ffi_decl->call);

  // Generate the call site itself.
  LLVMValueRef result;
  if(ffi_decl_can_error(ffi_decl))
  {
    // If the FFI declaration is marked as able to raise an exception,
    // generate an invoke instead of a call, which will unwind to the current
    // invoke target in the case that an exception is raised.
    pony_assert(c->frame->invoke_target != NULL);
    LLVMBasicBlockRef this_block = LLVMGetInsertBlock(c->builder);
    LLVMBasicBlockRef then_block =
      LLVMInsertBasicBlockInContext(c->context, this_block, "invoke");
    LLVMMoveBasicBlockAfter(then_block, this_block);
    LLVMBasicBlockRef else_block = c->frame->invoke_target;
    result = LLVMBuildInvoke_P(c->builder, ffi_decl->func,
      ffi_args, ffi_args_count, then_block, else_block, "");
    LLVMPositionBuilderAtEnd(c->builder, then_block);
  } else {
    // Otherwise, we generate a simple function call.
    result =
      LLVMBuildCall_P(c->builder, ffi_decl->func, ffi_args, ffi_args_count, "");
  }

  // If this function has an indirect return value, we load from the pointer
  // that we passed as the first argument - we expect the callee to have
  // stored the return value at that location.
  // We discard the actual call result value, which was void.
  if(ffi_decl_has_indirect_return(c, ffi_decl)) {
    pony_assert(LLVMTypeOf(result) == c->void_type);
    result = LLVMBuildLoad_P(c->builder, ffi_args[0], "ffi_indirect_return");
  }

  // We're now done with the arguments list (if any) that was allocated earlier.
  if (ffi_args != NULL)
    ponyint_pool_free_size(ffi_args_count * sizeof(LLVMValueRef), ffi_args);

  // Reset the debug info location to avoid later code having the wrong info.
  codegen_debugloc(c, NULL);

  // Apply any necessary additional transformations to the return value
  // that are needed to get the result value into the correct Pony type.
  result = ffi_decl_process_return_value(c, ffi_decl, result);

  return result;
}

LLVMValueRef gen_ffi(compile_t* c, ast_t* ast)
{
  // Get or generate the FFI declaration info structure, based on the call site.
  ffi_decl_t ffi_decl = { .decl = NULL, .func = NULL, .call = ast };
  if (!ffi_decl_get_or_gen(c, &ffi_decl))
    return NULL;
  pony_assert(ffi_decl.func != NULL);
  pony_assert(ffi_decl.decl != NULL);
  pony_assert(ffi_decl.call = ast);

  // Generate the call site and return the result value.
  return ffi_decl_gen_call(c, &ffi_decl);
}
