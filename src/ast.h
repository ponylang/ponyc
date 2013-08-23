#ifndef AST_H
#define AST_H

#include "lexer.h"
#include "symtab.h"

typedef struct ast_t
{
  token_t* t;
  symtab_t* symtab;
  struct ast_t* child;
  struct ast_t* sibling;
} ast_t;

ast_t* ast_new( token_id id, size_t line, size_t pos );
ast_t* ast_token( token_t* t );
void ast_reverse( ast_t* ast );
void ast_print( ast_t* ast );
void ast_free( ast_t* ast );

#endif
