#include "error.h"
#include "lexer.h"
#include "lexint.h"
#include "token.h"
#include "stringtab.h"
#include "../../libponyrt/mem/pool.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>


struct lexer_t
{
  source_t* source;
  errors_t* errors;

  // Information about next unused character in file
  size_t ptr;
  size_t len;
  size_t line;
  size_t pos;
  bool newline;

  // Position of current token
  size_t token_line;
  size_t token_pos;

  // Buffer containing current token text
  char* buffer;
  size_t buflen; // Length of buffer currently used
  size_t alloc;  // Space allocated for buffer
};


typedef struct lextoken_t
{
  const char* text;
  token_id id;
} lextoken_t;

#define MAX_SYMBOL_LENGTH 3

// Note that for symbols where one symbol starts with another, the longer one
// must appear first in this list.
// For example -> must appear before -
static const lextoken_t symbols[] =
{
  { "...", TK_ELLIPSIS },
  { "->", TK_ARROW },
  { "=>", TK_DBLARROW },

  { "<<", TK_LSHIFT },
  { ">>", TK_RSHIFT },

  { "==", TK_EQ },
  { "!=", TK_NE },

  { "<=", TK_LE },
  { ">=", TK_GE },

  { "{", TK_LBRACE },
  { "}", TK_RBRACE },
  { "(", TK_LPAREN },
  { ")", TK_RPAREN },
  { "[", TK_LSQUARE },
  { "]", TK_RSQUARE },
  { ",", TK_COMMA },

  { ".", TK_DOT },
  { "~", TK_TILDE },
  { ":", TK_COLON },
  { ";", TK_SEMI },
  { "=", TK_ASSIGN },

  { "+", TK_PLUS },
  { "-", TK_MINUS },
  { "*", TK_MULTIPLY },
  { "/", TK_DIVIDE },
  { "%", TK_MOD },
  { "@", TK_AT },

  { "<", TK_LT },
  { ">", TK_GT },

  { "|", TK_PIPE },
  { "&", TK_ISECTTYPE },
  { "^", TK_EPHEMERAL },
  { "!", TK_BORROWED },

  { "?", TK_QUESTION },
  { "-", TK_UNARY_MINUS },
  { "#", TK_CONSTANT },

  { "(", TK_LPAREN_NEW },
  { "[", TK_LSQUARE_NEW },
  { "-", TK_MINUS_NEW },

  { NULL, (token_id)0 }
};

static const lextoken_t keywords[] =
{
  { "_", TK_DONTCARE },
  { "compile_intrinsic", TK_COMPILE_INTRINSIC },

  { "use", TK_USE },
  { "type", TK_TYPE },
  { "interface", TK_INTERFACE },
  { "trait", TK_TRAIT },
  { "primitive", TK_PRIMITIVE },
  { "struct", TK_STRUCT },
  { "class", TK_CLASS },
  { "actor", TK_ACTOR },
  { "object", TK_OBJECT },
  { "lambda", TK_LAMBDA },

  { "delegate", TK_DELEGATE },
  { "as", TK_AS },
  { "is", TK_IS },
  { "isnt", TK_ISNT },

  { "var", TK_VAR },
  { "let", TK_LET },
  { "embed", TK_EMBED },
  { "new", TK_NEW },
  { "fun", TK_FUN },
  { "be", TK_BE },

  { "iso", TK_ISO },
  { "trn", TK_TRN },
  { "ref", TK_REF },
  { "val", TK_VAL },
  { "box", TK_BOX },
  { "tag", TK_TAG },

  { "this", TK_THIS },
  { "return", TK_RETURN },
  { "break", TK_BREAK },
  { "continue", TK_CONTINUE },
  { "consume", TK_CONSUME },
  { "recover", TK_RECOVER },

  { "if", TK_IF },
  { "ifdef", TK_IFDEF },
  { "then", TK_THEN },
  { "else", TK_ELSE },
  { "elseif", TK_ELSEIF },
  { "end", TK_END },
  { "for", TK_FOR },
  { "in", TK_IN },
  { "while", TK_WHILE },
  { "do", TK_DO },
  { "repeat", TK_REPEAT },
  { "until", TK_UNTIL },
  { "match", TK_MATCH },
  { "where", TK_WHERE },
  { "try", TK_TRY },
  { "with", TK_WITH },
  { "error", TK_ERROR },
  { "compile_error", TK_COMPILE_ERROR },

  { "not", TK_NOT },
  { "and", TK_AND },
  { "or", TK_OR },
  { "xor", TK_XOR },

  { "digestof", TK_DIGESTOF },
  { "addressof", TK_ADDRESS },
  { "__loc", TK_LOCATION },

  { "true", TK_TRUE },
  { "false", TK_FALSE },

  // #keywords.
  { "#read", TK_CAP_READ },
  { "#send", TK_CAP_SEND },
  { "#share", TK_CAP_SHARE },
  { "#alias", TK_CAP_ALIAS },
  { "#any", TK_CAP_ANY },

  // $keywords, for testing only.
  { "$noseq", TK_TEST_NO_SEQ },
  { "$scope", TK_TEST_SEQ_SCOPE },
  { "$try_no_check", TK_TEST_TRY_NO_CHECK },
  { "$borrowed", TK_TEST_BORROWED },
  { "$updatearg", TK_TEST_UPDATEARG },
  { "$extra", TK_TEST_EXTRA },
  { "$ifdefand", TK_IFDEFAND },
  { "$ifdefor", TK_IFDEFOR },
  { "$ifdefnot", TK_IFDEFNOT },
  { "$flag", TK_IFDEFFLAG },
  { "$let", TK_MATCH_CAPTURE },

  { NULL, (token_id)0 }
};

static const lextoken_t abstract[] =
{
  { "x", TK_NONE }, // Needed for AST printing

  { "program", TK_PROGRAM },
  { "package", TK_PACKAGE },
  { "module", TK_MODULE },

  { "members", TK_MEMBERS },
  { "fvar", TK_FVAR },
  { "flet", TK_FLET },
  { "ffidecl", TK_FFIDECL },
  { "fficall", TK_FFICALL },

  { "provides", TK_PROVIDES },
  { "uniontype", TK_UNIONTYPE },
  //{ "isecttype", TK_ISECTTYPE },  // Now treated as the symbol '&'
  { "tupletype", TK_TUPLETYPE },
  { "nominal", TK_NOMINAL },
  { "thistype", TK_THISTYPE },
  { "funtype", TK_FUNTYPE },
  { "lambdatype", TK_LAMBDATYPE },
  { "infer", TK_INFERTYPE },
  { "errortype", TK_ERRORTYPE },

  { "iso (bind)", TK_ISO_BIND },
  { "trn (bind)", TK_TRN_BIND },
  { "ref (bind)", TK_REF_BIND },
  { "val (bind)", TK_VAL_BIND },
  { "box (bind)", TK_BOX_BIND },
  { "tag (bind)", TK_TAG_BIND },

  { "#read (bind)", TK_CAP_READ_BIND },
  { "#send (bind)", TK_CAP_SEND_BIND },
  { "#share (bind)", TK_CAP_SHARE_BIND },
  { "#alias (bind)", TK_CAP_ALIAS_BIND },
  { "#any (bind)", TK_CAP_ANY_BIND },

  { "literal", TK_LITERAL },
  { "branch", TK_LITERALBRANCH },
  { "opliteral", TK_OPERATORLITERAL },

  { "typeparams", TK_TYPEPARAMS },
  { "typeparam", TK_TYPEPARAM },
  { "valueformalparam", TK_VALUEFORMALPARAM },
  { "params", TK_PARAMS },
  { "param", TK_PARAM },
  { "typeargs", TK_TYPEARGS },
  { "valueformalarg", TK_VALUEFORMALARG },
  { "positionalargs", TK_POSITIONALARGS },
  { "namedargs", TK_NAMEDARGS },
  { "namedarg", TK_NAMEDARG },
  { "updatearg", TK_UPDATEARG },
  { "lambdacaptures", TK_LAMBDACAPTURES },
  { "lambdacapture", TK_LAMBDACAPTURE },

  { "seq", TK_SEQ },
  { "qualify", TK_QUALIFY },
  { "call", TK_CALL },
  { "tuple", TK_TUPLE },
  { "array", TK_ARRAY },
  { "cases", TK_CASES },
  { "case", TK_CASE },
  { "try", TK_TRY_NO_CHECK },

  { "reference", TK_REFERENCE },
  { "packageref", TK_PACKAGEREF },
  { "typeref", TK_TYPEREF },
  { "typeparamref", TK_TYPEPARAMREF },
  { "newref", TK_NEWREF },
  { "newberef", TK_NEWBEREF },
  { "beref", TK_BEREF },
  { "funref", TK_FUNREF },
  { "fvarref", TK_FVARREF },
  { "fletref", TK_FLETREF },
  { "embedref", TK_EMBEDREF },
  { "varref", TK_VARREF },
  { "letref", TK_LETREF },
  { "paramref", TK_PARAMREF },
  { "newapp", TK_NEWAPP },
  { "beapp", TK_BEAPP },
  { "funapp", TK_FUNAPP },

  { "\\n", TK_NEWLINE },
  {NULL, (token_id)0}
};


static bool allow_test_symbols = false;


void lexer_allow_test_symbols()
{
  allow_test_symbols = true;
}


// Report an error at the specified location
static void lex_error_at(lexer_t* lexer, size_t line, size_t pos,
  const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  errorv(lexer->errors, lexer->source, line, pos, fmt, ap);
  va_end(ap);
}


// Report an error for the current token
static void lex_error(lexer_t* lexer, const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  errorv(lexer->errors, lexer->source, lexer->token_line, lexer->token_pos,
    fmt, ap);
  va_end(ap);
}


static bool is_eof(lexer_t* lexer)
{
  return lexer->len == 0;
}


// Append the given character to the current token text
static void append_to_token(lexer_t* lexer, char c)
{
  if(lexer->buflen >= lexer->alloc)
  {
    size_t new_len = (lexer->alloc > 0) ? lexer->alloc << 1 : 64;
    char* new_buf = (char*)ponyint_pool_alloc_size(new_len);
    memcpy(new_buf, lexer->buffer, lexer->alloc);

    if(lexer->alloc > 0)
      ponyint_pool_free_size(lexer->alloc, lexer->buffer);

    lexer->buffer = new_buf;
    lexer->alloc = new_len;
  }

  lexer->buffer[lexer->buflen] = c;
  lexer->buflen++;
}


// Make a token with the specified ID and no token text
static token_t* make_token(lexer_t* lexer, token_id id)
{
  token_t* t = token_new(id);
  token_set_pos(t, lexer->source, lexer->token_line, lexer->token_pos);
  return t;
}


// Make a token with the specified ID and current token text
static token_t* make_token_with_text(lexer_t* lexer, token_id id)
{
  token_t* t = make_token(lexer, id);

  if(lexer->buffer == NULL) // No text for token
    token_set_string(t, stringtab(""), 0);
  else
    token_set_string(t, stringtab_len(lexer->buffer, lexer->buflen),
      lexer->buflen);

  return t;
}


/* Consume the specified number of characters from our source.
 * Only the first character may be a newline.
 */
static void consume_chars(lexer_t* lexer, size_t count)
{
  assert(lexer->len >= count);

  if(count == 0)
    return;

  if(lexer->source->m[lexer->ptr] == '\n')
  {
    lexer->line++;
    lexer->pos = 0;
  }

  lexer->ptr += count;
  lexer->len -= count;
  lexer->pos += count;
}


// Look at the next unused character in our source, without consuming it
static char look(lexer_t* lexer)
{
  if(is_eof(lexer))
    return '\0';

  return lexer->source->m[lexer->ptr];
}


// look(lexer) is equivalent to lookn(lexer, 1)
static char lookn(lexer_t* lexer, size_t chars)
{
  if(lexer->len < chars)
    return '\0';

  return lexer->source->m[lexer->ptr + chars - 1];
}


// Report that the current literal token doesn't terminate
static token_t* literal_doesnt_terminate(lexer_t* lexer)
{
  lex_error(lexer, "Literal doesn't terminate");
  lexer->ptr += lexer->len;
  lexer->len = 0;
  return make_token(lexer, TK_LEX_ERROR);
}


// Process a block comment the leading / * for which has been seen, but not
// consumed
static token_t* nested_comment(lexer_t* lexer)
{
  consume_chars(lexer, 2); // Leading / *
  size_t depth = 1;

  while(depth > 0)
  {
    if(lexer->len <= 1)
    {
      lex_error(lexer, "Nested comment doesn't terminate");
      lexer->ptr += lexer->len;
      lexer->len = 0;
      return make_token(lexer, TK_LEX_ERROR);
    }

    if(look(lexer) == '*' && lookn(lexer, 2) == '/')
    {
      consume_chars(lexer, 2);
      depth--;
    }
    else if(look(lexer) == '/' && lookn(lexer, 2) == '*')
    {
      consume_chars(lexer, 2);
      depth++;
    }
    else
    {
      consume_chars(lexer, 1);
    }
  }

  lexer->newline = false;
  return NULL;
}


// Process a line comment the leading // for which has been seen, but not
// consumed
static token_t* line_comment(lexer_t* lexer)
{
  consume_chars(lexer, 2); // Leading //

  // We don't consume the terminating newline here, but it will be handled next
  // as whitespace
  while(!is_eof(lexer) && (look(lexer) != '\n'))
    consume_chars(lexer, 1);

  return NULL;
}


// Process a slash, which has been seen, but not consumed
static token_t* slash(lexer_t* lexer)
{
  if(lookn(lexer, 2) == '*')
    return nested_comment(lexer);

  if(lookn(lexer, 2) == '/')
    return line_comment(lexer);

  consume_chars(lexer, 1);
  return make_token(lexer, TK_DIVIDE);
}


/**
* Removes longest common prefix indentation from every line in a triple
* quoted string. If the string begins with an empty line, that line is removed
* entirely.
*/
static void normalise_string(lexer_t* lexer)
{
  if(lexer->buflen == 0)
    return;

  // If we aren't multiline, do nothing.
  if(memchr(lexer->buffer, '\n', lexer->buflen) == NULL)
    return;

  // Calculate leading whitespace.
  char* buf = lexer->buffer;
  size_t ws = lexer->buflen;
  size_t ws_this_line = 0;
  bool in_leading_ws = true;

  for(size_t i = 0; i < lexer->buflen; i++)
  {
    char c = lexer->buffer[i];

    if(in_leading_ws)
    {
      if(c == ' ' || c == '\t')
      {
        ws_this_line++;
      }
      else if((c != '\r') && (c != '\n'))
      {
        if(ws_this_line < ws)
          ws = ws_this_line;

        in_leading_ws = false;
      }
    }

    if(c == '\n')
    {
      ws_this_line = 0;
      in_leading_ws = true;
    }
  }

  // Trim leading whitespace on each line.
  if(ws > 0)
  {
    char* line_start = lexer->buffer;
    char* compacted = lexer->buffer;
    size_t rem = lexer->buflen;

    while(rem > 0)
    {
      char* line_end = (char*)memchr(line_start, '\n', rem);
      size_t line_len =
        (line_end == NULL) ? rem : (size_t)(line_end - line_start + 1);

      if(line_start != line_end)
      {
        size_t trim = (line_len < ws) ? line_len : ws;
        memmove(compacted, line_start + trim, line_len - trim);
        compacted += line_len - trim;
      }
      else
      {
        memmove(compacted, line_start, line_len);
        compacted += line_len;
      }

      line_start += line_len;
      rem -= line_len;
    }

    lexer->buflen = compacted - lexer->buffer;
  }

  // Trim a leading newline if there is one.
  buf = lexer->buffer;

  if((buf[0] == '\r') && (buf[1] == '\n'))
  {
    lexer->buflen -= 2;
    memmove(&buf[0], &buf[2], lexer->buflen);
  }
  else if(buf[0] == '\n')
  {
    lexer->buflen--;
    memmove(&buf[0], &buf[1], lexer->buflen);
  }
}


// Process a triple quoted string, the leading """ of which has been seen, but
// not consumed
static token_t* triple_string(lexer_t* lexer)
{
  consume_chars(lexer, 3);  // Leading """

  while(true)
  {
    if(is_eof(lexer))
      return literal_doesnt_terminate(lexer);

    char c = look(lexer);

    if((c == '\"') && (lookn(lexer, 2) == '\"') && (lookn(lexer, 3) == '\"'))
    {
      consume_chars(lexer, 3);

      // Triple strings can end with 3 or more "s. If there are more than 3
      // the extra ones are part of the string contents
      while(look(lexer) == '\"')
      {
        append_to_token(lexer, '\"');
        consume_chars(lexer, 1);
      }

      normalise_string(lexer);
      return make_token_with_text(lexer, TK_STRING);
    }

    consume_chars(lexer, 1);
    append_to_token(lexer, c);
  }
}


// Read a hex or unicode escape sequence, for which the leading \x has been
// consumed.
// The length specified is the number of hex digits expected.
// On success return the unicode value.
// On error return minus the number of characters processed (including the \x)
// and do not report an error.
static int read_hex_escape(lexer_t* lexer, int length)
{
  uint32_t value = 0;

  int text_len = 2; // start with "\x"

  for(int i = 0; i < length; i++)
  {
    char c = look(lexer);
    int digit = 0;

    if((c >= '0') && (c <= '9'))
      digit = c - '0';
    else if((c >= 'a') && (c <= 'f'))
      digit = c + 10 - 'a';
    else if((c >= 'A') && (c <= 'F'))
      digit = c + 10 - 'A';
    else
      return -text_len;

    text_len++;
    consume_chars(lexer, 1);
    value = (value << 4) + digit;
  }

  return value;
}


// Process a string or character escape sequence, the leading \ of which has
// been seen but not consumed.
// Errors are reported at the start of the sequence (ie the \ ).
// Returns the escape value or <0 on error.
static int escape(lexer_t* lexer, bool unicode_allowed)
{
  // Record the start position of the escape sequence for error reporting
  const char* start = &lexer->source->m[lexer->ptr];
  size_t line = lexer->line;
  size_t pos = lexer->pos;

  char c = lookn(lexer, 2);
  consume_chars(lexer, 2);
  int value = -2; // Default is 2 bad characters, \ and whatever follows it
  int hex_digits = 0;

  switch(c)
  {
    case 'a':  value = 0x07; break;
    case 'b':  value = 0x08; break;
    case 'e':  value = 0x1B; break;
    case 'f':  value = 0x0C; break;
    case 'n':  value = 0x0A; break;
    case 'r':  value = 0x0D; break;
    case 't':  value = 0x09; break;
    case 'v':  value = 0x0B; break;
    case '\"': value = 0x22; break;
    case '\'': value = 0x27; break;
    case '\\': value = 0x5C; break;
    case '0':  value = 0x00; break;
    case 'x': hex_digits = 2; break;

    case 'u':
      if(unicode_allowed)
        hex_digits = 4;
      break;

    case 'U':
      if(unicode_allowed)
        hex_digits = 6;
      break;
  }

  if(hex_digits > 0)
  {
    value = read_hex_escape(lexer, hex_digits);

    if(value < 0)
    {
      lex_error_at(lexer, line, pos,
        "Invalid escape sequence \"%.*s\", %d hex digits required",
        -value, start, hex_digits);
      return -1;
    }

    if(value > 0x10FFFF)
    {
      lex_error_at(lexer, line, pos,
        "Escape sequence \"%8s\" exceeds unicode range (0x10FFFF)", start);
      return -1;
    }
  }

  if(value < 0)
  {
    lex_error_at(lexer, line, pos, "Invalid escape sequence \"%.*s\"",
      -value, start);

    return -1;
  }

  return value;
}


// Append the given value to the current token text, UTF-8 encoded
static void append_utf8(lexer_t* lexer, int value)
{
  assert(value >= 0 && value <= 0x10FFFF);

  if(value <= 0x7F)
  {
    append_to_token(lexer, (char)(value & 0x7F));
  }
  else if(value <= 0x7FF)
  {
    append_to_token(lexer, (char)(0xC0 | (value >> 6)));
    append_to_token(lexer, (char)(0x80 | (value & 0x3F)));
  }
  else if(value <= 0xFFFF)
  {
    append_to_token(lexer, (char)(0xE0 | (value >> 12)));
    append_to_token(lexer, (char)(0x80 | ((value >> 6) & 0x3F)));
    append_to_token(lexer, (char)(0x80 | (value & 0x3F)));
  }
  else
  {
    append_to_token(lexer, (char)(0xF0 | (value >> 18)));
    append_to_token(lexer, (char)(0x80 | ((value >> 12) & 0x3F)));
    append_to_token(lexer, (char)(0x80 | ((value >> 6) & 0x3F)));
    append_to_token(lexer, (char)(0x80 | (value & 0x3F)));
  }
}


// Process a string literal, the leading " of which has been seen, but not
// consumed
static token_t* string(lexer_t* lexer)
{
  if((lookn(lexer, 2) == '\"') && (lookn(lexer, 3) == '\"'))
    return triple_string(lexer);

  consume_chars(lexer, 1);  // Leading "

  while(true)
  {
    if(is_eof(lexer))
      return literal_doesnt_terminate(lexer);

    char c = look(lexer);

    if(c == '"')
    {
      consume_chars(lexer, 1);
      return make_token_with_text(lexer, TK_STRING);
    }

    if(c == '\\')
    {
      int value = escape(lexer, true);

      // Just ignore bad escapes here and carry on. They've already been
      // reported and this allows catching later errors.
      if(value >= 0)
        append_utf8(lexer, value);
    }
    else
    {
      append_to_token(lexer, c);
      consume_chars(lexer, 1);
    }
  }
}


// Process a character literal, the leading ' of which has been seen, but not
// consumed
static token_t* character(lexer_t* lexer)
{
  consume_chars(lexer, 1);  // Leading '

  lexint_t value;
  lexint_zero(&value);

  while(true)
  {
    if(is_eof(lexer))
      return literal_doesnt_terminate(lexer);

    int c = look(lexer);

    if(c == '\'')
    {
      consume_chars(lexer, 1);
      token_t* t = make_token(lexer, TK_INT);
      token_set_int(t, &value);
      return t;
    }

    if(c == '\\')
      c = escape(lexer, false);
    else
      consume_chars(lexer, 1);

    // Just ignore bad escapes here and carry on. They've already been
    // reported and this allows catching later errors.
    if(c >= 0)
      lexint_char(&value, c);

    // TODO: Should we catch overflow and treat as an error?
  }
}


/** Process an integral literal or integral part of a real.
 * No digits have yet been consumed.
 * There must be at least one digit present.
 * Return true on success, false on failure.
 * The end_on_e flag indicates that we treat e (or E) as a valid terminator
 * character, rather than part of the integer being processed.
 * The given context is used in error reporting.
 * The value read is added onto the end of any existing value in out_value.
 */
static bool lex_integer(lexer_t* lexer, uint32_t base,
  lexint_t* out_value, uint32_t* out_digit_count, bool end_on_e,
  const char* context)
{
  uint32_t digit_count = 0;

  while(!is_eof(lexer))
  {
    char c = look(lexer);
    uint32_t digit = 0;

    if(c == '_')
    {
      // Ignore underscores in numbers
      consume_chars(lexer, 1);
      continue;
    }

    if(end_on_e && ((c == 'e') || (c == 'E')))
      break;

    if((c >= '0') && (c <= '9'))
      digit = c - '0';
    else if((c >= 'a') && (c <= 'z'))
      digit = c - 'a' + 10;
    else if((c >= 'A') && (c <= 'Z'))
      digit = c - 'A' + 10;
    else
      break;

    if(digit >= base)
    {
      lex_error(lexer, "Invalid character in %s: %c", context, c);
      return false;
    }

    if(!lexint_accum(out_value, digit, base))
    {
      lex_error(lexer, "overflow in numeric literal");
      return false;
    }

    consume_chars(lexer, 1);
    digit_count++;
  }

  if(digit_count == 0)
  {
    lex_error(lexer, "No digits in %s", context);
    return false;
  }

  if(out_digit_count != NULL)
    *out_digit_count = digit_count;

  return true;
}


// Process a real literal, the leading integral part has already been read.
// The . or e has been seen but not consumed.
static token_t* real(lexer_t* lexer, lexint_t* integral_value)
{
  lexint_t significand = *integral_value;

  lexint_t e;
  lexint_zero(&e);
  bool exp_neg = false;

  uint32_t mantissa_digit_count = 0;
  char c = look(lexer);
  assert(c == '.' || c == 'e' || c == 'E');

  if(c == '.')
  {
    c = lookn(lexer, 2);

    if(c < '0' || c > '9')
    {
      // Treat this as an integer token followed by a dot token
      token_t* t = make_token(lexer, TK_INT);
      token_set_int(t, integral_value);
      return t;
    }

    consume_chars(lexer, 1);  // Consume dot

    // Read in rest of the significand
    if(!lex_integer(lexer, 10, &significand, &mantissa_digit_count, true,
      "real number mantissa"))
      return make_token(lexer, TK_LEX_ERROR);
  }

  if((look(lexer) == 'e') || (look(lexer) == 'E'))
  {
    consume_chars(lexer, 1);  // Consume e

    if((look(lexer) == '+') || (look(lexer) == '-'))
    {
      exp_neg = (look(lexer) == '-');
      consume_chars(lexer, 1);
    }

    if(!lex_integer(lexer, 10, &e, NULL, false,
      "real number exponent"))
      return make_token(lexer, TK_LEX_ERROR);
  }

  token_t* t = make_token(lexer, TK_FLOAT);

  double ds = lexint_double(&significand);
  double de = lexint_double(&e);

  // Note that we must negate the exponent (if required) before applying the
  // mantissa digit count offset.
  if(exp_neg)
    de = -de;

  de -= mantissa_digit_count;
  token_set_float(t, ds * pow(10.0, de));
  return t;
}


// Process a non-decimal number literal, the leading base specifier of which
// has already been consumed
static token_t* nondecimal_number(lexer_t* lexer, int base,
  const char* context)
{
  lexint_t value;
  lexint_zero(&value);

  if(!lex_integer(lexer, base, &value, NULL, false, context))
    return make_token(lexer, TK_LEX_ERROR);

  token_t* t = make_token(lexer, TK_INT);
  token_set_int(t, &value);
  return t;
}


// Process a number literal, the first character of which has been seen but not
// consumed
static token_t* number(lexer_t* lexer)
{
  if(look(lexer) == '0')
  {
    switch(lookn(lexer, 2))
    {
      case 'x':
      case 'X':
        consume_chars(lexer, 2);  // Consume 0x
        return nondecimal_number(lexer, 16, "hexadecimal number");

      case 'b':
      case 'B':
        consume_chars(lexer, 2);  // Consume 0b
        return nondecimal_number(lexer, 2, "binary number");

      default: {}
    }
  }

  // Decimal
  lexint_t value;
  lexint_zero(&value);

  if(!lex_integer(lexer, 10, &value, NULL, true, "decimal number"))
    return make_token(lexer, TK_LEX_ERROR);

  if((look(lexer) == '.') || (look(lexer) == 'e') || (look(lexer) == 'E'))
    return real(lexer, &value);

  token_t* t = make_token(lexer, TK_INT);
  token_set_int(t, &value);
  return t;
}


// Read an identifer into the current token text buffer, but don't consume the
// characters from the source yet.
// Return value is the length of the read id.
static size_t read_id(lexer_t* lexer)
{
  size_t len = 0;
  char c;

  while(true)
  {
    c = lookn(lexer, len + 1);

    if((c != '_') && (c != '\'') && !isalnum(c))
      break;

    append_to_token(lexer, c);
    len++;
  }

  // Add a nul terminator to our name so we can use strcmp(), but don't count
  // it in the text length
  append_to_token(lexer, '\0');
  lexer->buflen--;

  return len;
}


// Process a keyword or identifier, possibly with a special prefix (eg '#').
// Any prefix must have been consumed.
// If no keyword is found the allow_identifiers parameter specifies whether an
// identifier token should be created.
// A lone prefix will not match as an identifier.
// Both keywords and identifiers are greedy, consuming all legal characters.
// Returns NULL if no match found.
static token_t* keyword(lexer_t* lexer, bool allow_identifiers)
{
  size_t len = read_id(lexer);

  for(const lextoken_t* p = keywords; p->text != NULL; p++)
  {
    if(!strcmp(lexer->buffer, p->text))
    {
      consume_chars(lexer, len);
      return make_token(lexer, p->id);
    }
  }

  if(allow_identifiers && len > 0)
  {
    consume_chars(lexer, len);
    return make_token_with_text(lexer, TK_ID);
  }

  return NULL;
}


// Process a hash, which has been seen, but not consumed.
static token_t* hash(lexer_t* lexer)
{
  append_to_token(lexer, look(lexer));  // #
  consume_chars(lexer, 1);
  token_t* t = keyword(lexer, false);

  if(t != NULL)
    return t;

  // No hash keyword found, just return the hash.
  return make_token(lexer, TK_CONSTANT);
}


// Process a dollar, which has been seen, but not consumed.
static token_t* dollar(lexer_t* lexer)
{
  append_to_token(lexer, look(lexer));  // $
  consume_chars(lexer, 1);

  if(allow_test_symbols)
  {
    // Test mode, allow test keywords and identifiers.
    // Note that a lone '$' is always an error to allow tests to force a lexer
    // error.
    token_t* t = keyword(lexer, true);

    if(t != NULL)
      return t;
  }

  // No test keyword or identifier found. Either we have just a lone '$' or
  // we're not in test mode so no dollar symbols are allowed.
  lex_error(lexer, "Unrecognized character: $");
  return make_token(lexer, TK_LEX_ERROR);
}


// Modify the given token to its newline form, if it is on a newline
static token_id newline_symbols(token_id raw_token, bool newline)
{
  if(!newline)
    return raw_token;

  switch(raw_token)
  {
    case TK_LPAREN:  return TK_LPAREN_NEW;
    case TK_LSQUARE: return TK_LSQUARE_NEW;
    case TK_MINUS:   return TK_MINUS_NEW;
    default:         return raw_token;
  }
}


// Process a symbol the leading character of which has been seen, but not
// consumed
static token_t* symbol(lexer_t* lexer)
{
  char sym[MAX_SYMBOL_LENGTH];

  for(size_t i = 0; i < sizeof(sym); ++i)
    sym[i] = lookn(lexer, i + 1);

  for(const lextoken_t* p = symbols; p->text != NULL; p++)
  {
    const char* symbol = p->text;

    for(int i = 0; symbol[i] == '\0' || symbol[i] == sym[i]; ++i)
    {
      if(symbol[i] == '\0')
      {
        consume_chars(lexer, i);
        return make_token(lexer, newline_symbols(p->id, lexer->newline));
      }
    }
  }

  lex_error(lexer, "Unrecognized character: %c", sym[0]);
  consume_chars(lexer, 1);
  return make_token(lexer, TK_LEX_ERROR);
}


lexer_t* lexer_open(source_t* source, errors_t* errors)
{
  assert(source != NULL);

  lexer_t* lexer = POOL_ALLOC(lexer_t);
  memset(lexer, 0, sizeof(lexer_t));

  lexer->source = source;
  lexer->errors = errors;
  lexer->len = source->len;
  lexer->line = 1;
  lexer->pos = 1;
  lexer->newline = true;

  return lexer;
}


void lexer_close(lexer_t* lexer)
{
  if(lexer == NULL)
    return;

  if(lexer->buffer != NULL)
    ponyint_pool_free_size(lexer->alloc, lexer->buffer);

  POOL_FREE(lexer_t, lexer);
}


token_t* lexer_next(lexer_t* lexer)
{
  assert(lexer != NULL);

  token_t* t = NULL;

  while(t == NULL)
  {
    lexer->token_line = lexer->line;
    lexer->token_pos = lexer->pos;
    lexer->buflen = 0;

    if(is_eof(lexer))
    {
      t = make_token(lexer, TK_EOF);
      break;
    }

    char c = look(lexer);

    switch(c)
    {
      case '\n':
        lexer->newline = true;
        consume_chars(lexer, 1);
        break;

      case '\r':
      case '\t':
      case ' ':
        consume_chars(lexer, 1);
        break;

      case '/':
        t = slash(lexer);
        break;

      case '\"':
        t = string(lexer);
        break;

      case '\'':
        t = character(lexer);
        break;

      case '#':
        t = hash(lexer);
        break;

      case '$':
        t = dollar(lexer);
        break;

      default:
        if(isdigit(c))
        {
          t = number(lexer);
        }
        else if(isalpha(c) || (c == '_'))
        {
          t = keyword(lexer, true);
          assert(t != NULL);
        }
        else
        {
          t = symbol(lexer);
        }
    }
  }

  lexer->newline = false; // We've found a symbol, so no longer a new line
  return t;
}


const char* lexer_print(token_id id)
{
  for(const lextoken_t* p = abstract; p->text != NULL; p++)
  {
    if(id == p->id)
      return p->text;
  }

  for(const lextoken_t* p = keywords; p->text != NULL; p++)
  {
    if(id == p->id)
      return p->text;
  }

  for(const lextoken_t* p = symbols; p->text != NULL; p++)
  {
    if(id == p->id)
      return p->text;
  }

  return NULL;
}
