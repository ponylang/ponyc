#ifndef REACH_H
#define REACH_H

#include "../ast/ast.h"
#include "../pass/pass.h"
#include "../codegen/gendebug.h"
#include "../../libponyrt/ds/hash.h"
#include "../../libponyrt/ds/stack.h"

#include <llvm-c/Core.h>

PONY_EXTERN_C_BEGIN

typedef struct reachable_method_t reachable_method_t;
typedef struct reachable_method_name_t reachable_method_name_t;
typedef struct reachable_field_t reachable_field_t;
typedef struct reachable_type_t reachable_type_t;

DECLARE_STACK(reachable_method_stack, reachable_method_stack_t,
  reachable_method_t);
DECLARE_HASHMAP(reachable_methods, reachable_methods_t,
  reachable_method_t);
DECLARE_HASHMAP(reachable_method_names, reachable_method_names_t,
  reachable_method_name_t);
DECLARE_HASHMAP(reachable_types, reachable_types_t, reachable_type_t);
DECLARE_HASHMAP(reachable_type_cache, reachable_type_cache_t,
  reachable_type_t);

struct reachable_method_t
{
  const char* name;
  const char* full_name;
  token_id cap;
  ast_t* typeargs;
  ast_t* r_fun;
  uint32_t vtable_index;

  LLVMTypeRef func_type;
  LLVMValueRef func;
  LLVMValueRef func_handler;
  LLVMMetadataRef di_method;
  LLVMMetadataRef di_file;
  bool intrinsic;
  reachable_method_t* subordinate;

  size_t param_count;
  reachable_type_t** params;
  reachable_type_t* result;
};

struct reachable_method_name_t
{
  token_id cap;
  const char* name;
  reachable_methods_t r_methods;
};

struct reachable_field_t
{
  ast_t* ast;
  reachable_type_t* type;
  bool embed;
};

struct reachable_type_t
{
  const char* name;
  const char* mangle;
  ast_t* ast;
  token_id underlying;

  reachable_method_names_t methods;
  reachable_type_cache_t subtypes;
  uint32_t type_id;
  uint32_t vtable_size;

  LLVMTypeRef structure;
  LLVMTypeRef structure_ptr;
  LLVMTypeRef primitive;
  LLVMTypeRef use_type;

  LLVMTypeRef desc_type;
  LLVMValueRef desc;
  LLVMValueRef instance;
  LLVMValueRef trace_fn;
  LLVMValueRef serialise_fn;
  LLVMValueRef deserialise_fn;
  LLVMValueRef final_fn;
  LLVMValueRef dispatch_fn;
  LLVMValueRef dispatch_switch;

  LLVMMetadataRef di_file;
  LLVMMetadataRef di_type;
  LLVMMetadataRef di_type_embed;

  uint32_t field_count;
  reachable_field_t* fields;
};

typedef struct
{
  reachable_types_t types;
  reachable_method_stack_t* stack;
  uint32_t next_type_id;
} reach_t;

/// Allocate a new set of reachable types.
reach_t* reach_new();

/// Free a set of reachable types.
void reach_free(reach_t* r);

/** Determine code reachability for a method in a type.
 *
 * The type should be a nominal, including any typeargs. The supplied method
 * typeargs can be NULL if there are none.
 */
void reach(reach_t* r, ast_t* type, const char* name, ast_t* typeargs,
  pass_opt_t* opt);

reachable_type_t* reach_type(reach_t* r, ast_t* type);

reachable_type_t* reach_type_name(reach_t* r, const char* name);

reachable_method_t* reach_method(reachable_type_t* t, token_id cap,
  const char* name, ast_t* typeargs);

reachable_method_name_t* reach_method_name(reachable_type_t* t,
  const char* name);

uint32_t reach_vtable_index(reachable_type_t* t, const char* name);

void reach_dump(reach_t* r);

PONY_EXTERN_C_END

#endif
