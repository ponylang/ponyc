#ifndef PACKAGE_H
#define PACKAGE_H

#include "ast.h"

ast_t* package_load( ast_t* from, const char* path );

#endif
