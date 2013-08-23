#ifndef PACKAGE_H
#define PACKAGE_H

#include "ast.h"
#include <stdbool.h>

bool package_start( const char* path );
bool package_load( ast_t* from, const char* path );

#endif
