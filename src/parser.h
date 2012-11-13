#ifndef PARSER_H
#define PARSER_H

#include "error.h"

typedef struct ast_t ast_t;
typedef struct parser_t parser_t;

parser_t* parser_open( const char* file );
errorlist_t* parser_errors( parser_t* parser );
void parser_close( parser_t* parser );

#endif
