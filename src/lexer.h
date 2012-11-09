#ifndef LEXER_H
#define LEXER_H

typedef struct lexer_t lexer_t;
typedef struct token_t token_t;

typedef enum
{
  TK_ERROR,
  TK_COMMENT,
  TK_STRING,
  TK_INT,
  TK_FLOAT,
  TK_ID,
  TK_TYPEID,
  TK_DIVIDE,
} token_id;

lexer_t* lexer_open( const char* file );
void lexer_close( lexer_t* lexer );

token_t* lexer_next( lexer_t* lexer );
void token_free( token_t* token );

#endif
