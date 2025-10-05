#ifndef PASS_DARTSRC_H
#define PASS_DARTSRC_H

#include <platform.h>
#include "../ast/ast.h"
#include "pass.h"

PONY_EXTERN_C_BEGIN

typedef struct DartModule {
  ast_t* program;
} DartModule;

typedef struct DartModule *DartModuleRef;


bool DartPrintModuleToFile(DartModuleRef M, const char *Filename,
                               char **ErrorMessage);

void DartDisposeMessage(char *Message);

PONY_EXTERN_C_END

#endif
