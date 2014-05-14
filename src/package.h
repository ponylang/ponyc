#ifndef PACKAGE_H
#define PACKAGE_H

#include "ast.h"

void package_init(const char* name);

ast_t* package_load(ast_t* from, const char* path);

void package_done();

#endif
