#ifndef CODEGEN_DWARF_H
#define CODEGEN_DWARF_H

#include "codegen.h"
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

void dwarf_program(compile_t*c, ast_t* program);

PONY_EXTERN_C_END

#endif
