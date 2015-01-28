#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "source.h"

bool pass_parse(ast_t* package, source_t* source);

// Flags to communicate extra info from parser to syntax pass
#define AST_IN_PARENS   1
#define TEST_ONLY       2
#define LAST_ON_LINE    4
#define MISSING_SEMI    8

#endif
