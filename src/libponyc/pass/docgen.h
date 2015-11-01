#ifndef DOCGEN_H
#define DOCGEN_H

#include <platform.h>
#include "../ast/ast.h"
#include "pass.h"

PONY_EXTERN_C_BEGIN

// Generate docs for the given program
void generate_docs(ast_t* program, pass_opt_t* options);

PONY_EXTERN_C_END

#endif
