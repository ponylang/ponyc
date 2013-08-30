#include "lexer.h"
#include "stringtab.h"
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
  source_t* source;
  size_t ptr;
  size_t len;

  size_t line;
  size_t pos;

  char* buffer;
  size_t buflen;
  size_t alloc;
};

typedef struct symbol_t
{
  const char* symbol;
  token_id id;
} symbol_t;

static const symbol_t symbols2[] =
{
  { "->", TK_ARROW },

  { "<<", TK_LSHIFT },
  { ">>", TK_RSHIFT },

  { "==", TK_EQ },
  { "!=", TK_NEQ },

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

  { ".", TK_DOT },
  { ":", TK_COLON },
  { ";", TK_SEMI },
  { "=", TK_EQUALS },

  { "+", TK_PLUS },
  { "-", TK_MINUS },
  { "*", TK_MULTIPLY },
  { "/", TK_DIVIDE },
  { "%", TK_MOD },

  { "<", TK_LT },
  { ">", TK_GT },

  { "|", TK_PIPE },
  { "&", TK_AMP },

  { NULL, 0 }
};

static const symbol_t keywords[] =
{
  { "use", TK_USE },
  { "as", TK_AS },
  { "alias", TK_ALIAS },
  { "trait", TK_TRAIT },
  { "class", TK_CLASS },
  { "actor", TK_ACTOR },
  { "is", TK_IS },
  { "iso", TK_ISO },
  { "var", TK_VAR },
  { "val", TK_VAL },
  { "tag", TK_TAG },
  { "fun", TK_FUN },
  { "msg", TK_MSG },
  { "return", TK_RETURN },
  { "break", TK_BREAK },
  { "continue", TK_CONTINUE },
  { "if", TK_IF },
  { "then", TK_THEN },
  { "else", TK_ELSE },
  { "end", TK_END },
  { "for", TK_FOR },
  { "in", TK_IN },
  { "while", TK_WHILE },
  { "do", TK_DO },
  { "match", TK_MATCH },
  { "try", TK_TRY },
  { "throw", TK_THROW },
  { "this", TK_THIS },
  { "not", TK_NOT },
  { "and", TK_AND },
  { "or", TK_OR },
  { "xor", TK_XOR },
  { "private", TK_PRIVATE },
  { "infer", TK_INFER },

  { NULL, 0 }
};

static const symbol_t abstract[] =
{
  { "program", TK_PROGRAM },
  { "package", TK_PACKAGE },
  { "module", TK_MODULE },
  { "list", TK_LIST },
  { "adt", TK_ADT },
  { "objecttype", TK_OBJECTTYPE },
  { "functiontype", TK_FUNCTIONTYPE },
  { "mode", TK_MODE },
  { "param", TK_PARAM },
  { "call", TK_CALL },
  { "seq", TK_SEQ },

  { NULL, 0 }
};

static void lexerror( lexer_t* lexer, const char* fmt, ... )
{
  va_list ap;
  va_start( ap, fmt );
  errorv( lexer->source, lexer->line, lexer->pos, fmt, ap );
  va_end( ap );
}

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
  return lexer->source->m[lexer->ptr];
}

static void string_terminate( lexer_t* lexer )
{
  lexerror( lexer, "String doesn't terminate" );
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
  char* m = &lexer->source->m[lexer->ptr];
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
      lexerror( lexer, "Escape sequence contains non-hexadecimal %c", c );
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
    lexerror( lexer, "Escape sequence exceeds unicode range (0x10FFFF)" );
    return false;
  }

  return true;
}

static const char* copy( lexer_t* lexer )
{
  if( lexer->buflen == 0 ) { return NULL; }
  append( lexer, '\0' );

  const char* str = stringtab( lexer->buffer );
  lexer->buflen = 0;

  return str;
}

static token_t* token( lexer_t* lexer )
{
  return token_new( 0, lexer->line, lexer->pos );
}

static void newline( lexer_t* lexer )
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
      lexerror( lexer, "Nested comment doesn't terminate" );
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
      newline( lexer );
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

static token_t* slash( lexer_t* lexer )
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

  token_t* t = token(lexer );
  t->id = TK_DIVIDE;

  return t;
}

static token_t* string( lexer_t* lexer )
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

      token_t* t = token( lexer );
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
        lexerror( lexer, "Invalid escape sequence: \\%c", c );
      }
    } else {
      append( lexer, look( lexer ) );
      adv( lexer, 1 );
    }
  }

  return NULL;
}

static token_t* real( lexer_t* lexer, __int128_t v )
{
  int digits = 0;
  int e = 0;
  bool error = false;
  char c;

  if( look( lexer ) == '.' )
  {
    adv( lexer, 1 );

    while( lexer->len > 0 )
    {
      c = look( lexer );

      if( (c >= '0') && (c <= '9') )
      {
        v = (v * 10) + (c - '0');
        digits++;
        e--;
      } else if( (c == 'e') || (c == 'E') ) {
        break;
      } else if( c == '_' ) {
        // skip
      } else if( isalpha( c ) ) {
        if( !error )
        {
          lexerror( lexer, "Invalid digit in real number: %c", c );
          error = true;
        }
      } else {
        break;
      }

      adv( lexer, 1 );
    }

    if( digits == 0 )
    {
      lexerror( lexer, "Real number has no digits following '.'" );
      error = true;
    }
  }

  if( (lexer->len > 0) && ((look( lexer ) == 'e') || (look( lexer ) == 'E')) )
  {
    adv( lexer, 1 );
    digits = 0;

    if( lexer->len == 0 )
    {
      lexerror( lexer, "Real number doesn't terminate" );
      return NULL;
    }

    c = look( lexer );
    bool neg = false;
    int n = 0;

    if( (c == '+') || (c == '-') )
    {
      adv( lexer, 1 );
      neg = (c == '-');

      if( lexer->len == 0 )
      {
        lexerror( lexer, "Real number doesn't terminate" );
        return NULL;
      }
    }

    while( lexer->len > 0 )
    {
      c = look( lexer );

      if( (c >= '0') && (c <= '9') )
      {
        n = (n * 10) + (c - '0');
        digits++;
      } else if( c == '_' ) {
        // skip
      } else if( isalpha( c ) ) {
        if( !error )
        {
          lexerror( lexer, "Invalid digit in exponent: %c", c );
          error = true;
        }
      } else {
        break;
      }

      adv( lexer, 1 );
    }

    if( neg )
    {
      e -= n;
    } else {
      e += n;
    }

    if( digits == 0 )
    {
      lexerror( lexer, "Exponent has no digits" );
      error = true;
    }
  }

  if( error ) { return NULL; }

  token_t* t = token( lexer );
  t->id = TK_FLOAT;
  t->real = v * pow( 10.0, e );
  return t;
}

static token_t* hexadecimal( lexer_t* lexer )
{
  __int128_t v = 0;
  bool error = false;
  char c;

  while( lexer->len > 0 )
  {
    c = look( lexer );

    if( (c >= '0') && (c <= '9') )
    {
      v = (v * 16) + (c - '0');
    } else if( (c >= 'a') && (c <= 'z') ) {
      v = (v * 16) + (c - 'a');
    } else if( (c >= 'A') && (c <= 'Z') ) {
      v = (v * 16) + (c - 'A');
    } else if( c == '_' ) {
      // skip
    } else if( isalpha( c ) ) {
      if( !error )
      {
        lexerror( lexer, "Invalid character in hexadecimal number: %c", c );
        error = true;
      }
    } else {
      break;
    }

    adv( lexer, 1 );
  }

  if( error ) { return NULL; }

  token_t* t = token( lexer );
  t->id = TK_INT;
  t->integer = v;
  return t;
}

static token_t* decimal( lexer_t* lexer )
{
  __int128_t v = 0;
  bool error = false;
  char c;

  while( lexer->len > 0 )
  {
    c = look( lexer );

    if( (c >= '0') && (c <= '9') )
    {
      v = (v * 10) + (c - '0');
    } else if( (c == '.') || (c == 'e') || (c == 'E') ) {
      return real( lexer, v );
    } else if( c == '_' ) {
      // skip
    } else if( isalnum( c ) ) {
      if( !error )
      {
        lexerror( lexer, "Invalid character in decimal number: %c", c );
        error = true;
      }
    } else {
      break;
    }

    adv( lexer, 1 );
  }

  if( error ) { return NULL; }

  token_t* t = token( lexer );
  t->id = TK_INT;
  t->integer = v;
  return t;
}

static token_t* binary( lexer_t* lexer )
{
  __int128_t v = 0;
  bool error = false;
  char c;

  while( lexer->len > 0 )
  {
    c = look( lexer );

    if( (c >= '0') && (c <= '1') )
    {
      v = (v * 2) + (c - '0');
    } else if( c == '_' ) {
      // skip
    } else if( isalnum( c ) ) {
      if( !error )
      {
        lexerror( lexer, "Invalid character in binary number: %c", c );
        error = true;
      }
    } else {
      break;
    }

    adv( lexer, 1 );
  }

  if( error ) { return NULL; }

  token_t* t = token( lexer );
  t->id = TK_INT;
  t->integer = v;
  return t;
}

static token_t* number( lexer_t* lexer )
{
  if( look( lexer ) == '0' )
  {
    adv( lexer, 1 );

    if( lexer->len > 0 )
    {
      char c = look( lexer );

      switch( c )
      {
      case 'x':
        adv( lexer, 1 );
        return hexadecimal( lexer );

      case 'b':
        adv( lexer, 1 );
        return binary( lexer );

      default:
        return decimal( lexer );
      }
    }
  }

  return decimal( lexer );
}

static void read_id( lexer_t* lexer )
{
  char c;

  while( lexer->len > 0 )
  {
    c = look( lexer );

    if( (c == '_') || (c == '\'') || isalnum( c ) )
    {
      append( lexer, c );
      adv( lexer, 1 );
    } else {
      break;
    }
  }
}

static token_t* identifier( lexer_t* lexer )
{
  token_t* t = token( lexer );

  read_id( lexer );
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

  t->id = TK_ID;
  t->string = copy( lexer );
  return t;
}

static token_t* symbol( lexer_t* lexer )
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
          t = token( lexer );
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
      t = token( lexer );
      t->id = p->id;
      return t;
    }
  }

  lexerror( lexer, "Unknown symbol: %c", sym[0] );
  return NULL;
}

lexer_t* lexer_open( source_t* source )
{
  lexer_t* lexer = calloc( 1, sizeof(lexer_t) );
  lexer->source = source;
  lexer->len = source->len;
  lexer->line = 1;
  lexer->pos = 1;

  return lexer;
}

void lexer_close( lexer_t* lexer )
{
  if( lexer == NULL ) { return; }
  if( lexer->buffer != NULL ) { free( lexer->buffer ); }
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
      newline( lexer );
      adv( lexer, 1 );
      break;

    case '\r':
    case '\t':
    case ' ':
      adv( lexer, 1 );
      break;

    case '/':
      t = slash( lexer );
      break;

    case '\"':
      t = string( lexer );
      break;

    default:
      if( isdigit( c ) )
      {
        t = number( lexer );
      } else if( isalpha( c ) || (c == '_') ) {
        t = identifier( lexer );
      } else if( issymbol( c ) ) {
        t = symbol( lexer );
      } else {
        lexerror( lexer, "Unrecognized character: %c", c );
        adv( lexer, 1 );
      }
    }
  }

  if( t == NULL )
  {
    t = token( lexer );
    t->id = TK_EOF;
  }

  return t;
}

token_t* token_new( token_id id, size_t line, size_t pos )
{
  token_t* t = calloc( 1, sizeof(token_t) );
  t->id = id;
  t->line = line;
  t->pos = pos;

  return t;
}

const char* token_string( token_t* token )
{
  switch( token->id )
  {
  case TK_NONE:
    return "()";

  case TK_STRING:
  case TK_ID:
    return token->string;

  case TK_INT:
    return "INT";

  case TK_FLOAT:
    return "FLOAT";

  default: {}
  }

  for( const symbol_t* p = abstract; p->symbol != NULL; p++ )
  {
    if( token->id == p->id ) { return p->symbol; }
  }

  for( const symbol_t* p = keywords; p->symbol != NULL; p++ )
  {
    if( token->id == p->id ) { return p->symbol; }
  }

  for( const symbol_t* p = symbols1; p->symbol != NULL; p++ )
  {
    if( token->id == p->id ) { return p->symbol; }
  }

  for( const symbol_t* p = symbols2; p->symbol != NULL; p++ )
  {
    if( token->id == p->id ) { return p->symbol; }
  }

  return "UNKNOWN";
}

void token_free( token_t* token )
{
  if( token == NULL ) { return; }
  free( token );
}
