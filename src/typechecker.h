#ifndef TYPECHECKER_H
#define TYPECHECKER_H

#include "error.h"
#include "parser.h"

errorlist_t* typecheck( ast_t* ast );

#endif
