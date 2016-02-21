#ifndef USE_H
#define USE_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

/// Process a use command
/// @return true on success, false on failure
ast_result_t use_command(ast_t* ast, pass_opt_t* options);


typedef bool(*use_handler_t)(ast_t* use, const char* locator, ast_t* name,
  pass_opt_t* options);

// Set the test handler. This is for use only for testing.
void use_test_handler(use_handler_t handler, bool allow_alias,
  bool allow_guard);

PONY_EXTERN_C_END

#endif
