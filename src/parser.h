#ifndef PARSER_H
#define PARSER_H

typedef struct ast_t ast_t;
typedef struct parser_t parser_t;

parser_t* parser_open( const char* file );
void parser_close( parser_t* parser );

#endif
