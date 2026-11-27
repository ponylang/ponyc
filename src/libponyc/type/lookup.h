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

/** Whether a type names a private type in text written outside that type's
 * package, directly or through a type alias named outside it.
 */
bool type_names_foreign_private(pass_opt_t* opt, ast_t* type);

PONY_EXTERN_C_END

#endif
