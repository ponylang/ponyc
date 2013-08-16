#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "error.h"

typedef struct parser_t parser_t;

parser_t* parser_open( const char* file );
ast_t* parser_ast( parser_t* parser );
errorlist_t* parser_errors( parser_t* parser );
void parser_close( parser_t* parser );

#endif
