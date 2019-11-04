#ifndef REACH_H
#define REACH_H

#include "../ast/ast.h"
#include "../pass/pass.h"
#include "../type/reify.h"
#include "../../libponyrt/ds/hash.h"
#include "../../libponyrt/ds/stack.h"

PONY_EXTERN_C_BEGIN

typedef struct reach_method_t reach_method_t;
typedef struct reach_method_name_t reach_method_name_t;
typedef struct reach_field_t reach_field_t;
typedef struct reach_param_t reach_param_t;
typedef struct reach_type_t reach_type_t;

DECLARE_STACK(reach_method_stack, reach_method_stack_t, reach_method_t);
DECLARE_HASHMAP_SERIALISE(reach_methods, reach_methods_t, reach_method_t);
DECLARE_HASHMAP_SERIALISE(reach_mangled, reach_mangled_t, reach_method_t);
DECLARE_HASHMAP_SERIALISE(reach_method_names, reach_method_names_t,
  reach_method_name_t);
DECLARE_HASHMAP_SERIALISE(reach_types, reach_types_t, reach_type_t);
DECLARE_HASHMAP_SERIALISE(reach_type_cache, reach_type_cache_t, reach_type_t);

typedef void (*compile_opaque_free_fn)(void* p);

typedef struct compile_opaque_t
{
  compile_opaque_free_fn free_fn;
} compile_opaque_t;

struct reach_method_t
{
  const char* name;
  const char* mangled_name;
  const char* full_name;

  token_id cap;
  deferred_reification_t* fun;
  ast_t* typeargs;
  uint32_t vtable_index;
  bool needs_vtable_index;

  // Mark as true if the compiler supplies an implementation.
  bool intrinsic;

  // Mark as true if the compiler supplies an implementation and the function
  // isn't exposed to the user at all.
  bool internal;

  // Mark as true if the method is a forwarding method.
  bool forwarding;

  // Linked list of instantiations that use the same func.
  reach_method_t* subordinate;

  size_t param_count;
  reach_param_t* params;
  reach_type_t* result;

  compile_opaque_t* c_method;
};

struct reach_method_name_t
{
  token_id id;
  token_id cap;
  const char* name;
  reach_methods_t r_methods;
  reach_mangled_t r_mangled;
  bool internal;
};

struct reach_field_t
{
  ast_t* ast;
  reach_type_t* type;
  bool embed;
};

struct reach_param_t
{
  const char* name;
  ast_t* ast;
  reach_type_t* type;
  token_id cap;
};

struct reach_type_t
{
  const char* name;
  const char* mangle;
  ast_t* ast;
  ast_t* ast_cap;
  token_id underlying;

  reach_method_names_t methods;
  reach_method_t* bare_method;
  reach_type_cache_t subtypes;
  uint32_t type_id;
  uint32_t vtable_size;
  bool can_be_boxed;
  bool is_trait;
  uint32_t behaviour_index;

  uint32_t field_count;
  reach_field_t* fields;

  compile_opaque_t* c_type;
};

typedef struct reach_t
{
  reach_types_t types;
  reach_method_stack_t* method_stack;
  uint32_t object_type_count;
  uint32_t numeric_type_count;
  uint32_t tuple_type_count;
  uint32_t total_type_count;
  uint32_t trait_type_count;
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

reach_type_t* reach_type(reach_t* r, ast_t* type);

reach_type_t* reach_type_name(reach_t* r, const char* name);

reach_method_t* reach_method(reach_type_t* t, token_id cap,
  const char* name, ast_t* typeargs);

reach_method_name_t* reach_method_name(reach_type_t* t,
  const char* name);

uint32_t reach_vtable_index(reach_type_t* t, const char* name);

uint32_t reach_max_type_id(reach_t* r);

void reach_dump(reach_t* r);

pony_type_t* reach_method_pony_type();

pony_type_t* reach_method_name_pony_type();

pony_type_t* reach_field_pony_type();

pony_type_t* reach_param_pony_type();

pony_type_t* reach_type_pony_type();

pony_type_t* reach_pony_type();

PONY_EXTERN_C_END

#endif
