#ifndef USE_H
#define USE_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

/** Use commands are of the form:
 *   use "[scheme:]locator" [as name] [where condition]
 *
 * Each scheme has a handler function which must be registered before commands
 * can be processed. The first handler registered is used as the default for
 * commands that do not specify a scheme.
 */

typedef bool (*use_handler_t)(ast_t* use, const char* locator, ast_t* name,
  pass_opt_t* options);

/// Register the standard use handlers
void use_register_std();

/** Register a use handler. The first handler registered is the default.
 * @param scheme Scheme identifier string, including trailing :. String must be
 * valid indefinitely.
 */
void use_register_handler(const char* scheme, bool allow_name,
  use_handler_t handler);

/// Unregister and free all registered use handlers
void use_clear_handlers();

/// Process a use command
/// @return true on success, false on failure
bool use_command(ast_t* ast, pass_opt_t* options);

PONY_EXTERN_C_END

#endif
