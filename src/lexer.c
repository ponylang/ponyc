#include "lexer.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>

struct lexer_t
{
  char* m;
  size_t ptr;
  size_t len;

  size_t line;
  size_t pos;

  char* buffer;
  size_t buflen;
  size_t alloc;

  errorlist_t* errors;
};

typedef struct symbol_t
{
  const char* symbol;
  token_id id;
} symbol_t;

static const symbol_t symbols2[] =
{
  { "->", TK_RESULTS },
  { "::", TK_PACKAGE },

  { "<<", TK_LSHIFT },
  { ">>", TK_RSHIFT },

  { "==", TK_EQ },
  { "!=", TK_NOTEQ },
  { "#=", TK_STEQ },
  { "~=", TK_NSTEQ },

  { "<=", TK_LE },
  { ">=", TK_GE },

  { NULL, 0 }
};

static const symbol_t symbols1[] =
{
  { "{", TK_LBRACE },
  { "}", TK_RBRACE },
  { "(", TK_LPAREN },
  { ")", TK_RPAREN },
  { "[", TK_LBRACKET },
  { "]", TK_RBRACKET },
  { ",", TK_COMMA },

  { ".", TK_CALL },
  { ":", TK_OFTYPE },
  { "\\", TK_PARTIAL },
  { "=", TK_ASSIGN },
  { "!", TK_BANG },

  { "+", TK_PLUS },
  { "-", TK_MINUS },
  { "*", TK_MULTIPLY },
  { "/", TK_DIVIDE },
  { "%", TK_MOD },

  { "<", TK_LT },
  { ">", TK_GT },

  { "|", TK_OR },
  { "&", TK_AND },
  { "^", TK_XOR },

  { "@", TK_UNIQ },
  { "#", TK_READONLY },
  { "?", TK_RECEIVER },

  { NULL, 0 }
};

static const symbol_t keywords[] =
{
  { "use", TK_USE },
  { "declare", TK_DECLARE },
  { "type", TK_TYPE },
  { "lambda", TK_LAMBDA },
  { "trait", TK_TRAIT },
  { "object", TK_OBJECT },
  { "actor", TK_ACTOR },
  { "is", TK_IS },
  { "var", TK_VAR },
  { "delegate", TK_DELEGATE },
  { "new", TK_NEW },
  { "ambient", TK_AMBIENT },
  { "function", TK_FUNCTION },
  { "message", TK_MESSAGE },
  { "throws", TK_THROWS },
  { "throw", TK_THROW },
  { "return", TK_RETURN },
  { "break", TK_BREAK },
  { "continue", TK_CONTINUE },
  { "if", TK_IF },
  { "else", TK_ELSE },
  { "for", TK_FOR },
  { "in", TK_IN },
  { "while", TK_WHILE },
  { "do", TK_DO },
  { "match", TK_MATCH },
  { "case", TK_CASE },
  { "as", TK_AS },
  { "catch", TK_CATCH },
  { "always", TK_ALWAYS },
  { "this", TK_THIS },
  { "true", TK_TRUE },
  { "false", TK_FALSE },

  { NULL, 0 }
};

static bool issymbol( char c )
{
  return ((c >= '!') && (c <= '.'))
    || ((c >= ':') && (c <= '@'))
    || ((c >= '[') && (c <= '^'))
    || ((c >= '{') && (c <= '~'));
}

static void adv( lexer_t* lexer, size_t count )
{
  assert( lexer->len >= count );

  lexer->ptr += count;
  lexer->len -= count;
  lexer->pos += count;
}

static char look( lexer_t* lexer )
{
  assert( lexer->len > 0 );
  return lexer->m[lexer->ptr];
}

static char* copy( lexer_t* lexer )
{
  if( lexer->buflen == 0 ) { return NULL; }
  char* m = malloc( lexer->buflen + 1 );
  memcpy( m, lexer->buffer, lexer->buflen );
  m[lexer->buflen] = '\0';
  lexer->buflen = 0;

  return m;
}

static void string_terminate( lexer_t* lexer )
{
  error_new( lexer->errors, lexer->line, lexer->pos, "String doesn't terminate" );
  lexer->ptr += lexer->len;
  lexer->len = 0;
  lexer->buflen = 0;
}

static void append( lexer_t* lexer, char c )
{
  if( lexer->buflen >= lexer->alloc )
  {
    size_t nlen = (lexer->alloc > 0) ? lexer->alloc << 1 : 64;
    char* n = malloc( nlen );
    memcpy( n, lexer->buffer, lexer->alloc );
    free( lexer->buffer );
    lexer->buffer = n;
    lexer->alloc = nlen;
  }

  lexer->buffer[lexer->buflen] = c;
  lexer->buflen++;
}

static bool appendn( lexer_t* lexer, size_t len )
{
  char* m = &lexer->m[lexer->ptr];
  uint32_t c = 0;

  if( lexer->len < len )
  {
    string_terminate( lexer );
    return false;
  }

  adv( lexer, len );

  for( size_t i = 0; i < len; i++ )
  {
    c <<= 4;

    if( (m[i] >= '0') && (m[i] <= '9') )
    {
      c += m[i] - '0';
    } else if( (m[i] >= 'a') && (m[i] <= 'f') ) {
      c += m[i] - 'a';
    } else if( (m[i] >= 'A') && (m[i] <= 'F') ) {
      c += m[i] - 'a';
    } else {
      error_new( lexer->errors, lexer->line, lexer->pos, "Escape sequence contains non-hexadecimal %c", c );
      return false;
    }
  }

  if( c <= 0x7F )
  {
    append( lexer, c & 0x7F );
  } else if( c <= 0x7FF ) {
    append( lexer, 0xC0 | (c >> 6) );
    append( lexer, 0x80 | (c & 0x3F) );
  } else if( c <= 0xFFFF ) {
    append( lexer, 0xE0 | (c >> 12) );
    append( lexer, 0x80 | ((c >> 6) & 0x3F) );
    append( lexer, 0x80 | (c & 0x3F) );
  } else if( c <= 0x10FFFF ) {
    append( lexer, 0xF0 | (c >> 18) );
    append( lexer, 0x80 | ((c >> 12) & 0x3F) );
    append( lexer, 0x80 | ((c >> 6) & 0x3F) );
    append( lexer, 0x80 | (c & 0x3F) );
  } else {
    error_new( lexer->errors, lexer->line, lexer->pos, "Escape sequence exceeds unicode range (0x10FFFF)" );
    return false;
  }

  return true;
}

static token_t* token_new( lexer_t* lexer )
{
  token_t* t = calloc( 1, sizeof(token_t) );
  t->line = lexer->line;
  t->pos = lexer->pos;

  return t;
}

static void lexer_newline( lexer_t* lexer )
{
  lexer->line++;
  lexer->pos = 0;
}

static void nested_comment( lexer_t* lexer )
{
  size_t depth = 1;

  while( depth > 0 )
  {
    if( lexer->len <= 1 )
    {
      error_new( lexer->errors, lexer->line, lexer->pos, "Nested comment doesn't terminate" );
      lexer->ptr += lexer->len;
      lexer->len = 0;
      return;
    } if( look( lexer ) == '*' ) {
      adv( lexer, 1 );

      if( look( lexer ) == '/' )
      {
        depth--;
      }
    } else if( look( lexer ) == '/' ) {
      adv( lexer, 1 );

      if( look( lexer ) == '*' )
      {
        depth++;
      }
    } else if( look( lexer ) == '\n' ) {
      lexer_newline( lexer );
    }

    adv( lexer, 1 );
  }
}

static void line_comment( lexer_t* lexer )
{
  while( (lexer->len > 0) && (look( lexer ) != '\n') )
  {
    adv( lexer, 1 );
  }
}

static token_t* lexer_slash( lexer_t* lexer )
{
  adv( lexer, 1 );

  if( lexer->len > 0 )
  {
    if( look( lexer ) == '*' )
    {
      adv( lexer, 1 );
      nested_comment( lexer );
      return NULL;
    } else if( look( lexer ) == '/' ) {
      adv( lexer, 1 );
      line_comment( lexer );
      return NULL;
    }
  }

  token_t* t = token_new(lexer );
  t->id = TK_DIVIDE;

  return t;
}

static token_t* lexer_string( lexer_t* lexer )
{
  adv( lexer, 1 );
  assert( lexer->buflen == 0 );

  while( true )
  {
    if( lexer->len == 0 )
    {
      string_terminate( lexer );
      return NULL;
    } else if( look( lexer ) == '\"' ) {
      adv( lexer, 1 );

      token_t* t = token_new( lexer );
      t->id = TK_STRING;
      t->string = copy( lexer );
      return t;
    } else if( look( lexer ) == '\\' ) {
      if( lexer->len < 2 )
      {
        string_terminate( lexer );
        return NULL;
      }

      adv( lexer, 1 );
      char c = look( lexer );
      adv( lexer, 1 );

      switch( c )
      {
      case 'a':
        append( lexer, 0x07 );
        break;

      case 'b':
        append( lexer, 0x08 );
        break;

      case 'f':
        append( lexer, 0x0C );
        break;

      case 'n':
        append( lexer, 0x0A );
        break;

      case 'r':
        append( lexer, 0x0D );
        break;

      case 't':
        append( lexer, 0x09 );
        break;

      case 'v':
        append( lexer, 0x0B );
        break;

      case '\"':
        append( lexer, 0x22 );
        break;

      case '\\':
        append( lexer, 0x5C );
        break;

      case '0':
        append( lexer, 0x00 );
        break;

      case 'x':
        appendn( lexer, 2 );
        break;

      case 'u':
        appendn( lexer, 4 );
        break;

      case 'U':
        appendn( lexer, 6 );
        break;

      default:
        error_new( lexer->errors, lexer->line, lexer->pos, "Invalid escape sequence: \\%c", c );
      }
    } else {
      append( lexer, look( lexer ) );
      adv( lexer, 1 );
    }
  }

  return NULL;
}

static token_t* lexer_float( lexer_t* lexer, token_t* t, size_t v )
{
  double d = v;
  size_t places = 0;
  bool exp = false;
  bool expneg = false;
  int e = 0;

  if( !isdigit( look( lexer ) ) ) { return t; }

  while( lexer->len > 0 )
  {
    char c = look( lexer );

    if( isdigit( c ) )
    {
      adv( lexer, 1 );

      if( exp )
      {
        e = (e * 10) + (c - '0');
      } else {
        d = (d * 10) + (c - '0');
        places++;
      }
    } else if( !exp && ((c == 'e') || (c == 'E')) ) {
      adv( lexer, 1 );
      exp = true;
    } else if( exp && (e == 0) && (c == '+') ) {
      adv( lexer, 1 );
    } else if( exp && (e == 0) && (c == '-') ) {
      adv( lexer, 1 );
      expneg = true;
    } else {
      break;
    }
  }

  // FIX: error if places == 0 or exp && e == 0?
  t->id = TK_FLOAT;
  t->flt = d / (places * 10);

  if( exp )
  {
    if( expneg ) { e = -e; }
    t->flt *= pow( 10.0f, e );
  }

  return t;
}

static token_t* lexer_number( lexer_t* lexer )
{
  token_t* t = token_new( lexer );
  t->id = TK_INT;
  size_t v = 0;

  while( lexer->len > 0 )
  {
    char c = look( lexer );

    if( isdigit( c ) )
    {
      v = (v * 10) + (c - '0');
      adv( lexer, 1 );
    } else if( c == '.' ) {
      adv( lexer, 1 );
      return lexer_float( lexer, t, v );
    } else {
      break;
    }
  }

  t->integer = v;
  return t;
}

static token_t* lexer_id( lexer_t* lexer )
{
  token_t* t = token_new( lexer );
  t->id = TK_ID;
  assert( lexer->buflen == 0 );

  while( lexer->len > 0 )
  {
    char c = look( lexer );

    if( (c == '_') || isalnum( c ) )
    {
      append( lexer, c );
      adv( lexer, 1 );
    } else {
      break;
    }
  }

  append( lexer, '\0' );

  for( const symbol_t* p = keywords; p->symbol != NULL; p++ )
  {
    if( !strcmp( lexer->buffer, p->symbol ) )
    {
      t->id = p->id;
      lexer->buflen = 0;
      return t;
    }
  }

  t->string = copy( lexer );
  return t;
}

static token_t* lexer_typeid( lexer_t* lexer )
{
  token_t* t = token_new( lexer );
  t->id = TK_TYPEID;

  while( lexer->len > 0 )
  {
    char c = look( lexer );

    if( isalnum( c ) )
    {
      append( lexer, c );
      adv( lexer, 1 );
    } else {
      break;
    }
  }

  t->string = copy( lexer );
  return t;
}

static token_t* lexer_symbol( lexer_t* lexer )
{
  token_t* t;
  char sym[2];

  sym[0] = look( lexer );
  adv( lexer, 1 );

  if( lexer->len > 1 )
  {
    sym[1] = look( lexer );

    if( issymbol( sym[1] ) )
    {
      for( const symbol_t* p = symbols2; p->symbol != NULL; p++ )
      {
        if( (sym[0] == p->symbol[0]) && (sym[1] == p->symbol[1]) )
        {
          adv( lexer, 1 );
          t = token_new( lexer );
          t->id = p->id;
          return t;
        }
      }
    }
  }

  for( const symbol_t* p = symbols1; p->symbol != NULL; p++ )
  {
    if( sym[0] == p->symbol[0] )
    {
      t = token_new( lexer );
      t->id = p->id;
      return t;
    }
  }

  error_new( lexer->errors, lexer->line, lexer->pos, "Unknown symbol: %c", sym[0] );
  return NULL;
}

lexer_t* lexer_open( const char* file )
{
  lexer_t* lexer = calloc( 1, sizeof(lexer_t) );
  lexer->line = 1;
  lexer->errors = errorlist_new();

  FILE* fp = fopen( file, "rt" );

  if( fp == NULL )
  {
    error_new( lexer->errors, 0, 0, "Couldn't open %s", file );
    return lexer;
  }

  if( fseeko( fp, 0, SEEK_END ) != 0 )
  {
    error_new( lexer->errors, 0, 0, "Couldn't determine length of %s", file );
    fclose( fp );
    return lexer;
  }

  lexer->len = ftello( fp );

  if( lexer->len == -1 )
  {
    error_new( lexer->errors, 0, 0, "Couldn't determine length of %s", file );
    fclose( fp );
    return lexer;
  }

  if( fseeko( fp, 0, SEEK_SET ) != 0 )
  {
    error_new( lexer->errors, 0, 0, "Couldn't determine length of %s", file );
    fclose( fp );
    return lexer;
  }

  lexer->m = malloc( lexer->len );
  size_t r = fread( lexer->m, lexer->len, 1, fp );
  fclose( fp );

  if( r != 1 )
  {
    error_new( lexer->errors, 0, 0, "Couldn't determine length of %s", file );
    return lexer;
  }

  return lexer;
}

void lexer_close( lexer_t* lexer )
{
  if( lexer == NULL ) { return; }
  if( lexer->m != NULL ) { free( lexer->m ); }
  if( lexer->buffer != NULL ) { free( lexer->buffer ); }

  errorlist_free( lexer->errors );
  free( lexer );
}

token_t* lexer_next( lexer_t* lexer )
{
  token_t* t = NULL;

  while( (t == NULL) && (lexer->len > 0) )
  {
    char c = look( lexer );

    switch( c )
    {
    case '\n':
      lexer_newline( lexer );
      adv( lexer, 1 );
      break;

    case '\r':
    case '\t':
    case ' ':
      adv( lexer, 1 );
      break;

    case '/':
      t = lexer_slash( lexer );
      break;

    case '\"':
      t = lexer_string( lexer );
      break;

    default:
      if( isdigit( c ) )
      {
        t = lexer_number( lexer );
      } else if( islower( c ) || (c == '_') ) {
        t = lexer_id( lexer );
      } else if( isupper( c ) ) {
        t = lexer_typeid( lexer );
      } else if( issymbol( c ) ) {
        t = lexer_symbol( lexer );
      } else {
        error_new( lexer->errors, lexer->line, lexer->pos, "Unrecognized character: %c", c );
        adv( lexer, 1 );
      }
    }
  }

  if( t == NULL )
  {
    t = token_new( lexer );
    t->id = TK_EOF;
  }

  return t;
}

errorlist_t* lexer_errors( lexer_t* lexer )
{
  errorlist_t* e = lexer->errors;
  lexer->errors = NULL;
  return e;
}

void token_free( token_t* token )
{
  if( token == NULL ) { return; }

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
