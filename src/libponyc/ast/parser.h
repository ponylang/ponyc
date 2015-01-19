#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "source.h"

ast_t* parser(source_t* source);

// Flags to communicate extra info from parser to parsefix
#define AST_IN_PARENS   1
#define TEST_ONLY       2
#define LAST_ON_LINE    4
#define MISSING_SEMI    8

#endif
