#ifndef PACKAGE_H
#define PACKAGE_H

#include "ast.h"

void package_addpath( const char* path );

ast_t* package_load( ast_t* from, const char* path );

void package_done();

#endif
