#include "lexer.h"
#include <stdio.h>
#include <stdbool.h>
#include <malloc.h>
#include <assert.h>

struct lexer_t
{
  char* m;
  size_t ptr;
  size_t len;

  size_t line;
  size_t pos;
};

struct token_t
{
  token_id id;
  size_t line;
  size_t pos;

  union
  {
    char* string;
    double flt;
    size_t integer;
  };
};

static bool is_symbol( char c )
{
  return ((c >= '!') && (c <= '.'))
    || ((c >= ':') && (c <= '@'))
    || ((c >= '[') && (c <= '^'))
    || ((c >= '{') && (c <= '~'));
}

static char look( lexer_t* lexer )
{
  assert( lexer->len > 0 );
  return lexer->m[lexer->ptr];
}

static char lookahead( lexer_t* lexer )
{
  assert( lexer->len > 1 );
  return lexer->m[lexer->ptr + 1];
}

static token_t* token_new( lexer_t* lexer )
{
  token_t* t = malloc( sizeof(token_t) );
  t->line = lexer->line;
  t->pos = lexer->pos;
  t->string = NULL;

  return t;
}

static void lexer_adv( lexer_t* lexer, size_t count )
{
  assert( lexer->len >= count );

  lexer->ptr += count;
  lexer->len -= count;
  lexer->pos += count;
}

static void lexer_newline( lexer_t* lexer )
{
  lexer->line++;
  lexer->pos = 0;
}

static token_t* nested_comment( lexer_t* lexer )
{
  token_t* t = token_new( lexer );
  t->id = TK_COMMENT;
  size_t depth = 1;

  while( depth > 0 )
  {
    size_t adv = 1;

    if( lexer->len <= 1 )
    {
      t->id = TK_ERROR;
      break;
    } if( look( lexer ) == '*' ) {
      if( lookahead( lexer ) == '/' )
      {
        adv++;
        depth--;
      }
    } else if( look( lexer ) == '/' ) {
      if( lookahead( lexer ) == '*' )
      {
        adv++;
        depth++;
      }
    } else if( look( lexer ) == '\n' ) {
      lexer_newline( lexer );
    }

    lexer_adv( lexer, adv );
  }

  return t;
}

static token_t* line_comment( lexer_t* lexer )
{
  token_t* t = token_new( lexer );
  t->id = TK_COMMENT;

  while( (lexer->len > 0) && (look( lexer ) != '\n') )
  {
    lexer_adv( lexer, 1 );
  }

  return t;
}

static token_t* lexer_slash( lexer_t* lexer )
{
  if( lexer->len > 1 )
  {
    if( lookahead( lexer ) == '*' )
    {
      return nested_comment( lexer );
    } else if( lookahead( lexer ) == '/' ) {
      return line_comment( lexer );
    }
  }

  token_t* t = token_new(lexer );
  t->id = TK_DIVIDE;
  lexer_adv( lexer, 1 );

  return t;
}

static token_t* lexer_string( lexer_t* lexer )
{
  token_t* t = token_new( lexer );
  t->id = TK_STRING;
  lexer_adv( lexer, 1 );

  while( true )
  {
    if( lexer->len == 0 )
    {
      t->id = TK_ERROR;
      break;
    } else if( look( lexer ) == '\"' ) {
      lexer_adv( lexer, 1 );
      break;
    } else if( look( lexer ) == '\\' ) {
      // FIX: begin escape sequence
    } else {
      // FIX: append to string
    }
  }

  return t;
}

static token_t* lexer_number( lexer_t* lexer )
{
  // FIX:
  return NULL;
}

static token_t* lexer_id( lexer_t* lexer )
{
  token_t* t = token_new( lexer );
  t->id = TK_ID;

  while( lexer->len > 0 )
  {
    char c = look( lexer );

    if( (c == '_')
      || ((c >= 'a') && (c <= 'z'))
      || ((c >= 'A') && (c <= 'Z'))
      || ((c >= '0') && (c <= '9'))
      )
    {
      // FIX: append to id
      lexer_adv( lexer, 1 );
    } else {
      break;
    }
  }

  // FIX: check for a keyword

  return t;
}

static token_t* lexer_typeid( lexer_t* lexer )
{
  token_t* t = token_new( lexer );
  t->id = TK_TYPEID;

  while( lexer->len > 0 )
  {
    char c = look( lexer );

    if( ((c >= 'a') && (c <= 'z'))
      || ((c >= 'A') && (c <= 'Z'))
      || ((c >= '0') && (c <= '9'))
      )
    {
      // FIX: append to id
      lexer_adv( lexer, 1 );
    } else {
      break;
    }
  }

  return t;
}

static token_t* lexer_symbol( lexer_t* lexer )
{
  // FIX:
  return NULL;
}

lexer_t* lexer_open( const char* file )
{
  FILE* fp = fopen( file, "rt" );
  if( fp == NULL ) { return NULL; }

  if( fseek( fp, 0, SEEK_END ) != 0 )
  {
    fclose( fp );
    return NULL;
  }

  size_t flen = ftell( fp );
  fseek( fp, 0, SEEK_SET );
  char* m = malloc( flen );
  int r = fread( fp, flen, 1, fp );
  fclose( fp );

  if( r != 1 )
  {
    free( m );
    return NULL;
  }

  lexer_t* lexer = malloc( sizeof(lexer_t) );
  lexer->m = m;
  lexer->ptr = 0;
  lexer->len = flen;
  lexer->line = 1;
  lexer->pos = 0;

  return lexer;
}

void lexer_close( lexer_t* lexer )
{
  if( lexer == NULL ) { return; }
  if( lexer->m != NULL ) { free( lexer->m ); }
  free( lexer );
}

token_t* lexer_next( lexer_t* lexer )
{
  while( lexer->len > 0 )
  {
    char c = look( lexer );

    switch( c )
    {
    case '\n':
      lexer_newline( lexer );
      lexer_adv( lexer, 1 );
      break;

    case '\r':
    case '\t':
    case ' ':
      lexer_adv( lexer, 1 );
      break;

    case '/':
      return lexer_slash( lexer );

    case '\"':
      return lexer_string( lexer );

    default:
      if( (c >= '0') && (c <= '9') ) { return lexer_number( lexer ); }
      if( ((c >= 'a') && (c <= 'z')) || (c == '_') ) { return lexer_id( lexer ); }
      if( (c >= 'A') && (c <= 'Z') ) { return lexer_typeid( lexer ); }
      if( is_symbol( c ) ) { return lexer_symbol( lexer ); }

      token_t* t = token_new( lexer );
      t->id = TK_ERROR;
      return t;
    }
  }

  return NULL;
}

void token_free( token_t* token )
{
  switch( token->id )
  {
  case TK_STRING:
  case TK_ID:
  case TK_TYPEID:
    if( token->string != NULL ) { free( token->string ); }
    break;

  default: {}
  }

  free( token );
}
