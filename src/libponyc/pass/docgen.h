#ifndef DOCGEN_H
#define DOCGEN_H

#include <platform.h>
#include "../ast/ast.h"
#include "pass.h"

PONY_EXTERN_C_BEGIN

// Generate docs for the given program, tot he specified directory
void generate_docs(ast_t* ast, const char* dir);

PONY_EXTERN_C_END

#endif
