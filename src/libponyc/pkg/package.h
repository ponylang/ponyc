#ifndef PACKAGE_H
#define PACKAGE_H

#include "../ast/ast.h"

typedef struct package_t
{
  const char* path;
  size_t next_hygienic_id;
} package_t;

void package_init(const char* name);

void package_paths(const char* paths);

ast_t* program_load(const char* path);

ast_t* package_load(ast_t* from, const char* path);

ast_t* package_hygienic_id(ast_t* ast);

void package_done();

#endif
