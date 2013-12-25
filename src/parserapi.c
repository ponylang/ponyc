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

bool acceptalt( parser_t* parser, const token_id* id, ast_t* ast )
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

bool rulealt( parser_t* parser, const alt_t* alt, ast_t* ast )
{
  while( alt->f != NULL )
  {
    if( (alt->id == TK_NONE) || (current( parser ) == alt->id) )
    {
      ast_t* child = alt->f( parser );

      if( child == NULL )
      {
        return false;
      }

      ast_add( ast, child );
      return true;
    }

    alt++;
  }

  return false;
}

block_t block( parser_t* parser, const alt_t* alt, token_id name,
  token_id start, token_id stop, token_id sep, ast_t* ast )
{
  token_id tok_start[2] = { start, TK_NONE };
  token_id tok_stop[2] = { stop, TK_NONE };
  token_id tok_sep[2] = { sep, TK_NONE };

  ast_t* child;

  if( name != TK_NONE )
  {
    child = ast_new( name, parser->t->line, parser->t->pos, NULL );
    ast_add( ast, child );
  } else {
    child = ast;
  }

  if( (start != TK_NONE) && !acceptalt( parser, tok_start, NULL ) )
  {
    return BLOCK_NOTPRESENT;
  }

  bool have = false;

  while( true )
  {
    if( (stop != TK_NONE) && acceptalt( parser, tok_stop, NULL ) )
    {
      break;
    }

    if( !rulealt( parser, alt, child ) )
    {
      if( start != TK_NONE )
      {
        return BLOCK_ERROR;
      }
      break;
    }

    have = true;

    if( (sep != TK_NONE) && !acceptalt( parser, tok_sep, NULL ) )
    {
      if( (stop != TK_NONE) && !acceptalt( parser, tok_stop, NULL ) )
      {
        return BLOCK_ERROR;
      }
      break;
    }
  }

  return have ? BLOCK_OK : BLOCK_EMPTY;
}

ast_t* forwardalt( parser_t* parser, const alt_t* alt )
{
  while( alt->f != NULL )
  {
    if( (alt->id == TK_NONE) || (current( parser ) == alt->id) )
    {
      return alt->f( parser );
    }

    alt++;
  }

  return NULL;
}
