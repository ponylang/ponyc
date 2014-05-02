#include "parserapi.h"

static token_id current( parser_t* parser )
{
  return parser->t->id;
}

ast_t* consume( parser_t* parser )
{
  ast_t* ast = ast_token( parser->t );
  parser->t = lexer_next( parser->lexer );
  return ast;
}

void insert( parser_t* parser, token_id id, ast_t* ast )
{
  ast_t* child = ast_new( id, parser->t->line, parser->t->pos, NULL );
  ast_add( ast, child );
}

bool look( parser_t* parser, const token_id* id )
{
  while( *id != TK_NONE )
  {
    if( current( parser ) == *id )
    {
      return true;
    }

    id++;
  }

  return false;
}

bool accept( parser_t* parser, const token_id* id, ast_t* ast )
{
  if( !look( parser, id ) )
  {
    return false;
  }

  if( ast != NULL )
  {
    ast_t* child = ast_token( parser->t );
    ast_add( ast, child );
  } else {
    token_free( parser->t );
  }

  parser->t = lexer_next( parser->lexer );
  return true;
}

bool rulealt( parser_t* parser, const rule_t* alt, ast_t* ast )
{
  while( alt != NULL )
  {
    ast_t* child = alt( parser );

    if( child != NULL )
    {
      ast_add( ast, child );
      return true;
    }

    alt++;
  }

  return false;
}

ast_t* forwardalt( parser_t* parser, const rule_t* alt )
{
  while( alt != NULL )
  {
    ast_t* ast = alt( parser );

    if( ast != NULL )
    {
      return ast;
    }

    alt++;
  }

  return NULL;
}
