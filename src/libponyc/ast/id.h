#ifndef ID_H
#define ID_H

#include <platform.h>
#include "ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

/// Check that the name in the given ID node is valid for an entity.
/// If name is illegal an error will be generated.
bool check_id_type(pass_opt_t* opt, ast_t* id_node, const char* entity_desc);

/// Check that the name in the given ID node is valid for a type parameter.
/// If name is illegal an error will be generated.
bool check_id_type_param(pass_opt_t* opt, ast_t* id_node);

/// Check that the name in the given ID node is valid for a package.
/// If name is illegal an error will be generated.
bool check_id_package(pass_opt_t* opt, ast_t* id_node);

/// Check that the name in the given ID node is valid for a field.
/// If name is illegal an error will be generated.
bool check_id_field(pass_opt_t* opt, ast_t* id_node);

/// Check that the name in the given ID node is valid for a method.
/// If name is illegal an error will be generated.
bool check_id_method(pass_opt_t* opt, ast_t* id_node);

/// Check that the name in the given ID node is valid for a parameter.
/// If name is illegal an error will be generated.
bool check_id_param(pass_opt_t* opt, ast_t* id_node);

/// Check that the name in the given ID node is valid for a local.
/// If name is illegal an error will be generated.
bool check_id_local(pass_opt_t* opt, ast_t* id_node);

/// Report whether the given id name is a type name
bool is_name_type(const char* name);

// Report whether the given id name is private
bool is_name_private(const char* name);

// Report whether the given id name is an FFI declaration
bool is_name_ffi(const char* name);

// Report whether the given id name is for an internal test
bool is_name_internal_test(const char* name);

// Report whether the given id name is '_'
bool is_name_dontcare(const char* name);

PONY_EXTERN_C_END

#endif
