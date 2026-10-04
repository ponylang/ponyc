#ifndef TYPE_LOOKUP_H
#define TYPE_LOOKUP_H

#include <platform.h>
#include "reify.h"
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

// Find a member of a type by name. Reports an error when not found.
// Asserts if the type is a kind that cannot have members.
deferred_reification_t* lookup(pass_opt_t* opt, ast_t* from, ast_t* type,
  const char* name);

// Find a member of a type by name, returning NULL when the type has no
// such member or cannot have members. Reports no errors.
deferred_reification_t* lookup_try(pass_opt_t* opt, ast_t* from, ast_t* type,
  const char* name, bool allow_private);

// Compute the overload type suffix for a method. Format: "$<type1>$<type2>..."
// Uses deterministic encoding of nominal type names. Pass reify when the
// method's param types need reification; NULL when they are already concrete.
const char* overload_type_suffix(ast_t* method,
  deferred_reification_t* reify, pass_opt_t* opt);

// Parse an overload-suffixed name (e.g. "apply$6_String") into a base name
// and type suffix. If no '$', returns the original name and NULL suffix.
const char* overload_name_parse(const char* name, const char** out_suffix,
  pass_opt_t* opt);

// Given a TK_METHODGROUP and a type suffix (from overload_type_suffix),
// return the unguarded child whose parameter types match. Pass reify when
// the method's param types need reification; NULL when they are already
// concrete.
ast_t* methodgroup_select_by_suffix(ast_t* group, const char* suffix,
  deferred_reification_t* reify, pass_opt_t* opt);

PONY_EXTERN_C_END

#endif
