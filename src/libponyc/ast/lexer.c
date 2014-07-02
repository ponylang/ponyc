#include "lexer.h"
#include "../ds/stringtab.h"
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
  bool newline;

  char* buffer;  // Symbol text buffer
  size_t buflen; // Length of buffer currently used
  size_t alloc;  // Space allocated for buffe
};

#define LEX_ERROR  ((token_t*)1)

typedef struct lexsym_t
{
  const char* symbol;
  token_id id;
} lexsym_t;

static const lexsym_t symbols2[] =
{
  { "->", TK_ARROW },
  { "=>", TK_DBLARROW },

  { "<<", TK_LSHIFT },
  { ">>", TK_RSHIFT },

  { "==", TK_EQ },
  { "!=", TK_NE },

  { "<=", TK_LE },
  { ">=", TK_GE },

  { NULL, 0 }
};

static const lexsym_t symbols1[] =
{
  { "{", TK_LBRACE },
  { "}", TK_RBRACE },
  { "(", TK_LPAREN },
  { ")", TK_RPAREN },
  { "[", TK_LSQUARE },
  { "]", TK_RSQUARE },
  { ",", TK_COMMA },

  { ".", TK_DOT },
  { "!", TK_BANG },
  { ":", TK_COLON },
  { ";", TK_SEMI },
  { "=", TK_ASSIGN },

  { "+", TK_PLUS },
  { "-", TK_MINUS },
  { "*", TK_MULTIPLY },
  { "/", TK_DIVIDE },
  { "%", TK_MOD },
  { "^", TK_HAT },

  { "<", TK_LT },
  { ">", TK_GT },

  { "|", TK_PIPE },
  { "&", TK_AMP },
  { "?", TK_QUESTION },

  { "{", TK_LBRACE_NEW },
  { "(", TK_LPAREN_NEW },
  { "[", TK_LSQUARE_NEW },
  { "-", TK_MINUS_NEW },

  { NULL, 0 }
};

static const lexsym_t keywords[] =
{
  { "use", TK_USE },
  { "type", TK_TYPE },
  { "trait", TK_TRAIT },
  { "class", TK_CLASS },
  { "actor", TK_ACTOR },

  { "is", TK_IS },
  { "isnt", TK_ISNT },

  { "var", TK_VAR },
  { "let", TK_LET },
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
  { "as", TK_AS },
  { "where", TK_WHERE },
  { "try", TK_TRY },
  { "error", TK_ERROR },

  { "not", TK_NOT },
  { "and", TK_AND },
  { "or", TK_OR },
  { "xor", TK_XOR },

  { NULL, 0 }
};

static const lexsym_t abstract[] =
{
  { "program", TK_PROGRAM },
  { "package", TK_PACKAGE },
  { "module", TK_MODULE },

  { "members", TK_MEMBERS },
  { "fvar", TK_FVAR },
  { "flet", TK_FLET },

  { "types", TK_TYPES },
  { "uniontype", TK_UNIONTYPE },
  { "isecttype", TK_ISECTTYPE },
  { "tupletype", TK_TUPLETYPE },
  { "nominal", TK_NOMINAL },
  { "structural", TK_STRUCTURAL },
  { "thistype", TK_THISTYPE },

  { "typeparams", TK_TYPEPARAMS },
  { "typeparam", TK_TYPEPARAM },
  { "params", TK_PARAMS },
  { "param", TK_PARAM },
  { "typeargs", TK_TYPEARGS },
  { "positionalargs", TK_POSITIONALARGS },
  { "namedargs", TK_NAMEDARGS },
  { "namedarg", TK_NAMEDARG },

  { "seq", TK_SEQ },
  { "idseq", TK_IDSEQ },
  { "qualify", TK_QUALIFY },
  { "call", TK_CALL },
  { "tuple", TK_TUPLE },
  { "array", TK_ARRAY },
  { "object", TK_OBJECT },
  { "cases", TK_CASES },
  { "case", TK_CASE },

  { "reference", TK_REFERENCE },
  { "packageref", TK_PACKAGEREF },
  { "typeref", TK_TYPEREF },
  { "funref", TK_FUNREF },
  { "fieldref", TK_FIELDREF },
  { "paramref", TK_PARAMREF },
  { "localref", TK_LOCALREF },

  { NULL, 0 }
};


static void lexerror(lexer_t* lexer, const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  errorv(lexer->source, lexer->line, lexer->pos, fmt, ap);
  va_end(ap);
}


static bool is_eof(lexer_t* lexer)
{
  return lexer->len == 0;
}


static bool is_symbol_char(char c)
{
  return ((c >= '!') && (c <= '.'))
    || ((c >= ':') && (c <= '@'))
    || ((c >= '[') && (c <= '^'))
    || ((c >= '{') && (c <= '~'));
}


/* Advance our input by the specified number of characters.
 * Only the first character may be a newline.
 */
static void adv(lexer_t* lexer, size_t count)
{
  assert(lexer->len >= count);

  if(lexer->source->m[lexer->ptr] == '\n')
  {
    lexer->line++;
    lexer->pos = 0;
  }

  lexer->ptr += count;
  lexer->len -= count;
  lexer->pos += count;
}


static char look(lexer_t* lexer)
{
  if(is_eof(lexer))
    return '\0';

  return lexer->source->m[lexer->ptr];
}


static char look2(lexer_t* lexer)
{
  if(lexer->len <= 1)
    return '\0';

  return lexer->source->m[lexer->ptr + 1];
}


static void string_terminate(lexer_t* lexer)
{
  lexerror(lexer, "String doesn't terminate");
  lexer->ptr += lexer->len;
  lexer->len = 0;
  lexer->buflen = 0;
}


static void append(lexer_t* lexer, char c)
{
  if(lexer->buflen >= lexer->alloc)
  {
    size_t new_len = (lexer->alloc > 0) ? lexer->alloc << 1 : 64;
    char* new_buf = malloc(new_len);
    memcpy(new_buf, lexer->buffer, lexer->alloc);
    free(lexer->buffer);
    lexer->buffer = new_buf;
    lexer->alloc = new_len;
  }

  lexer->buffer[lexer->buflen] = c;
  lexer->buflen++;
}


static bool append_unicode(lexer_t* lexer, size_t len)
{
  char* m = &lexer->source->m[lexer->ptr];
  uint32_t c = 0;

  if(lexer->len < len)
  {
    string_terminate(lexer);
    return false;
  }

  adv(lexer, len);

  for(size_t i = 0; i < len; i++)
  {
    c <<= 4;

    if((m[i] >= '0') && (m[i] <= '9'))
    {
      c += m[i] - '0';
    } else if((m[i] >= 'a') && (m[i] <= 'f')) {
      c += m[i] + 10 - 'a';
    } else if((m[i] >= 'A') && (m[i] <= 'F')) {
      c += m[i] + 10 - 'A';
    } else {
      lexerror(lexer, "Escape sequence contains non-hexadecimal %c", c);
      return false;
    }
  }

  // UTF-8 encoding
  if(c <= 0x7F)
  {
    append(lexer, c & 0x7F);
  } else if(c <= 0x7FF) {
    append(lexer, 0xC0 | (c >> 6));
    append(lexer, 0x80 | (c & 0x3F));
  } else if(c <= 0xFFFF) {
    append(lexer, 0xE0 | (c >> 12));
    append(lexer, 0x80 | ((c >> 6) & 0x3F));
    append(lexer, 0x80 | (c & 0x3F));
  } else if(c <= 0x10FFFF) {
    append(lexer, 0xF0 | (c >> 18));
    append(lexer, 0x80 | ((c >> 12) & 0x3F));
    append(lexer, 0x80 | ((c >> 6) & 0x3F));
    append(lexer, 0x80 | (c & 0x3F));
  } else {
    lexerror(lexer, "Escape sequence exceeds unicode range (0x10FFFF)");
    return false;
  }

  return true;
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

  // make sure we have a null terminated string
  append(lexer, '\0');

  // if we aren't multiline, do nothing
  if(memchr(lexer->buffer, '\n', lexer->buflen) == NULL)
    return;

  // calculate leading whitespace
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
      else
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

  // trim leading whitespace on each line
  if(ws > 0)
  {
    char* line_start = lexer->buffer;
    char* compacted = lexer->buffer;
    size_t rem = lexer->buflen;

    while(rem > 0)
    {
      char* line_end = strchr(line_start, '\n');
      size_t line_len = (line_end == NULL) ? rem : line_end - line_start + 1;
      memmove(compacted, line_start + ws, line_len - ws);

      line_start += line_len;
      compacted += line_len - ws;
      rem -= line_len;
    }
  }

  // trim a leading newline if there is one
  buf = lexer->buffer;

  if((buf[0] == '\r') && (buf[1] == '\n'))
  {
    lexer->buflen -= 2;
    memmove(&buf[0], &buf[2], lexer->buflen);
  } else if(buf[0] == '\n') {
    lexer->buflen--;
    memmove(&buf[0], &buf[1], lexer->buflen);
  }
}


static const char* save_token_text(lexer_t* lexer)
{
  append(lexer, '\0');
  const char* str = stringtab(lexer->buffer);
  assert(str != NULL);
  lexer->buflen = 0;

  return str;
}


static token_t* token_new(token_id id, source_t* source, size_t line,
  size_t pos, bool newline)
{
  token_t* t = calloc(1, sizeof(token_t));
  t->newline = newline;
  t->source = source;
  t->line = line;
  t->pos = pos;

  token_setid(t, id);
  return t;
}


static token_t* token(lexer_t* lexer, token_id id)
{
  token_t* t = token_new(id, lexer->source, lexer->line, lexer->pos,
    lexer->newline);

  lexer->newline = false;
  return t;
}


static token_t* nested_comment(lexer_t* lexer)
{
  size_t depth = 1;

  while(depth > 0)
  {
    if(lexer->len <= 1)
    {
      lexerror(lexer, "Nested comment doesn't terminate");
      lexer->ptr += lexer->len;
      lexer->len = 0;
      return LEX_ERROR;
    }

    if(look(lexer) == '*' && look2(lexer) == '/')
    {
      adv(lexer, 2);
      depth--;
    }
    else if(look(lexer) == '/' && look2(lexer) == '*')
    {
      adv(lexer, 2);
      depth++;
    }
    else
    {
      adv(lexer, 1);
    }
  }

  return NULL;
}


static void line_comment(lexer_t* lexer)
{
  // We don't consume the terminating newline here, but it will be handled next
  // as whitespace
  while(!is_eof(lexer) && (look(lexer) != '\n'))
  {
    adv(lexer, 1);
  }
}


static token_t* slash(lexer_t* lexer)
{
  adv(lexer, 1);

  if(look(lexer) == '*')
  {
    adv(lexer, 1);
    return nested_comment(lexer);
  } else if(look(lexer) == '/') {
    adv(lexer, 1);
    line_comment(lexer);
    return NULL;
  }

  return token(lexer, TK_DIVIDE);
}


static token_t* triple_string(lexer_t* lexer)
{
  while(true)
  {
    if(is_eof(lexer))
    {
      string_terminate(lexer);
      return LEX_ERROR;
    }

    char c = look(lexer);
    adv(lexer, 1);

    if((c == '\"') && (look(lexer) == '\"') && (look2(lexer) == '\"'))
    {
      adv(lexer, 2);
      token_t* t = token(lexer, TK_STRING);
      normalise_string(lexer);
      t->string = save_token_text(lexer);
      assert(t->string != NULL);
      return t;
    }

    append(lexer, c);
  }
}


static token_t* string(lexer_t* lexer)
{
  adv(lexer, 1);  // Consume leading "
  assert(lexer->buflen == 0);

  if((look(lexer) == '\"') && (look2(lexer) == '\"'))
  {
    adv(lexer, 2);
    return triple_string(lexer);
  }

  while(true)
  {
    if(is_eof(lexer))
    {
      string_terminate(lexer);
      return LEX_ERROR;
    }

    char next_char = look(lexer);

    if(next_char == '\"')
    {
      adv(lexer, 1);
      token_t* t = token(lexer, TK_STRING);
      t->string = save_token_text(lexer);
      return t;
    }

    if(next_char == '\\')
    {
      if(lexer->len < 2)
      {
        string_terminate(lexer);
        return LEX_ERROR;
      }

      adv(lexer, 1);
      bool r = true;

      switch(look(lexer))
      {
      case 'a':  append(lexer, 0x07); break;
      case 'b':  append(lexer, 0x08); break;
      case 'e':  append(lexer, 0x1B); break;
      case 'f':  append(lexer, 0x0C); break;
      case 'n':  append(lexer, 0x0A); break;
      case 'r':  append(lexer, 0x0D); break;
      case 't':  append(lexer, 0x09); break;
      case 'v':  append(lexer, 0x0B); break;
      case '\"': append(lexer, 0x22); break;
      case '\\': append(lexer, 0x5C); break;
      case '0':  append(lexer, 0x00); break;

      case 'x':  r = append_unicode(lexer, 2); break;
      case 'u':  r = append_unicode(lexer, 4); break;
      case 'U':  r = append_unicode(lexer, 6); break;

      default:
        lexerror(lexer, "Invalid escape sequence: \\%c", look(lexer));
        r = false;
        break;
      }

      if(!r)
      {
        string_terminate(lexer);
        return LEX_ERROR;
      }
    } else {
      append(lexer, next_char);
      adv(lexer, 1);
    }
  }
}


/** Add the given digit to a literal value, checking for overflow.
 * Returns true on success, false on overflow error.
 */
static bool accum(lexer_t* lexer, __uint128_t* v, int digit, uint32_t base)
{
  __uint128_t v1 = *v;
  __uint128_t v2 = v1 * base;

  if((v2 / base) != v1)
  {
    lexerror(lexer, "overflow in numeric literal");
    return false;
  }

  v2 += digit;

  if(v2 < v1)
  {
    lexerror(lexer, "overflow in numeric literal");
    return false;
  }

  *v = v2;
  return true;
}


/** Process an integral literal or integral part of a real.
 * There must be at least one digit present.
 * Return true on success, false on failure.
 * The end_on_e flag indicates that we treat e (or E) as a valid terminator
 * character, rather than part of the integer being processed.
 * The given context is used in error reporting.
 * The value read is added onto the end of any existing value in out_value.
 */
static bool lex_integer(lexer_t* lexer, uint32_t base,
  __uint128_t* out_value, uint32_t* out_digit_count, bool end_on_e,
  const char* context)
{
  uint32_t digit_count = 0;

  while(!is_eof(lexer))
  {
    char c = look(lexer);
    uint32_t digit = 0;

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
      lexerror(lexer, "Invalid character in %s: %c", context, c);
      return false;
    }

    if(!accum(lexer, out_value, digit, base))
      return false;

    adv(lexer, 1);
    digit_count++;
  }

  if(digit_count == 0)
  {
    lexerror(lexer, "No digits in %s", context);
    return false;
  }

  if(out_digit_count != NULL)
    *out_digit_count = digit_count;

  return true;
}


/** Process a real literal when the leading integral part has already been
 * handled.
 */
static token_t* real(lexer_t* lexer, __uint128_t integral_value)
{
  __uint128_t significand = integral_value;
  __uint128_t e = 0;
  uint32_t mantissa_digit_count = 0;
  char c = look(lexer);
  assert(c == '.' || c == 'e' || c == 'E');

  if(c == '.')
  {
    c = look2(lexer);

    if(c < '0' || c > '9')
    {
      // Treat this as an integer token followed by a dot token
      token_t* t = token(lexer, TK_INT);
      t->integer = integral_value;
      return t;
    }

    adv(lexer, 1);  // Consume dot

    // Read in rest of the significand
    if(!lex_integer(lexer, 10, &significand, &mantissa_digit_count, true,
      "real number mantissa"))
      return LEX_ERROR;

    e = -mantissa_digit_count;
  }

  if((look(lexer) == 'e') || (look(lexer) == 'E'))
  {
    adv(lexer, 1);  // Consume e

    bool exp_neg = false;

    if((look(lexer) == '+') || (look(lexer) == '-'))
    {
      exp_neg = (look(lexer) == '-');
      adv(lexer, 1);
    }

    if(!lex_integer(lexer, 10, &e, NULL, false, "real number exponent"))
      return LEX_ERROR;

    if(exp_neg)
      e = -e;
  }

  e -= mantissa_digit_count;
  token_t* t = token(lexer, TK_FLOAT);
  t->real = significand * pow(10.0, e);
  return t;
}


static token_t* nondecimal_number(lexer_t* lexer, int base,
  const char* context)
{
  __uint128_t value = 0;
  if(!lex_integer(lexer, base, &value, NULL, false, context))
    return LEX_ERROR;

  token_t* t = token(lexer, TK_INT);
  t->integer = value;
  return t;
}


static token_t* number(lexer_t* lexer)
{
  if(look(lexer) == '0')
  {
    switch(look2(lexer))
    {
    case 'x':
    case 'X':
      adv(lexer, 2);  // Consume 0x
      return nondecimal_number(lexer, 16, "hexadecimal number");

    case 'b':
    case 'B':
      adv(lexer, 2);  // Consume 0b
      return nondecimal_number(lexer, 2, "binary number");
    }
  }

  // Decimal
  __uint128_t value = 0;
  if(!lex_integer(lexer, 10, &value, NULL, true, "decimal number"))
    return LEX_ERROR;

  if((look(lexer) == '.') || (look(lexer) == 'e') || (look(lexer) == 'E'))
    return real(lexer, value);

  token_t* t = token(lexer, TK_INT);
  t->integer = value;
  return t;
}


static void read_id(lexer_t* lexer)
{
  char c;

  while(!is_eof(lexer))
  {
    c = look(lexer);

    if((c == '_') || (c == '\'') || isalnum(c))
    {
      append(lexer, c);
      adv(lexer, 1);
    } else {
      break;
    }
  }

  append(lexer, '\0');
}


static token_t* identifier(lexer_t* lexer)
{
  read_id(lexer);

  for(const lexsym_t* p = keywords; p->symbol != NULL; p++)
  {
    if(!strcmp(lexer->buffer, p->symbol))
    {
      lexer->buflen = 0;
      return token(lexer, p->id);
    }
  }

  token_t* t = token(lexer, TK_ID);
  t->string = save_token_text(lexer);
  return t;
}


static token_t* symbol(lexer_t* lexer)
{
  char sym[2];

  sym[0] = look(lexer);
  adv(lexer, 1);
  sym[1] = look(lexer);

  if(is_symbol_char(sym[1]))
  {
    for(const lexsym_t* p = symbols2; p->symbol != NULL; p++)
    {
      if((sym[0] == p->symbol[0]) && (sym[1] == p->symbol[1]))
      {
        adv(lexer, 1);
        return token(lexer, p->id);
      }
    }
  }

  for(const lexsym_t* p = symbols1; p->symbol != NULL; p++)
  {
    if(sym[0] == p->symbol[0])
    {
      return token(lexer, p->id);
    }
  }

  lexerror(lexer, "Unknown symbol: %c", sym[0]);
  return LEX_ERROR;
}


lexer_t* lexer_open(source_t* source)
{
  lexer_t* lexer = calloc(1, sizeof(lexer_t));
  lexer->source = source;
  lexer->len = source->len;
  lexer->line = 1;
  lexer->pos = 1;

  return lexer;
}


void lexer_close(lexer_t* lexer)
{
  if(lexer == NULL)
    return;

  if(lexer->buffer != NULL)
    free(lexer->buffer);

  free(lexer);
}


token_t* lexer_next(lexer_t* lexer)
{
  token_t* t = NULL;
  size_t symbol_line;
  size_t symbol_pos;

  while(t == NULL)
  {
    if(is_eof(lexer))
      return token(lexer, TK_EOF);

    symbol_line = lexer->line;
    symbol_pos = lexer->pos;
    char c = look(lexer);

    switch(c)
    {
    case '\n':
      lexer->newline = true;
      adv(lexer, 1);
      break;

    case '\r':
    case '\t':
    case ' ':
      adv(lexer, 1);
      break;

    case '/':
      t = slash(lexer);
      break;

    case '\"':
      t = string(lexer);
      break;

    default:
      if(isdigit(c))
      {
        t = number(lexer);
      } else if(isalpha(c) || (c == '_')) {
        t = identifier(lexer);
      } else if(is_symbol_char(c)) {
        t = symbol(lexer);
      } else {
        lexerror(lexer, "Unrecognized character: %c", c);
        adv(lexer, 1);
      }
    }
  }

  if(t == LEX_ERROR)
    return NULL;

  t->line = symbol_line;
  t->pos = symbol_pos;

  return t;
}


token_t* token_blank(token_id id)
{
  return token_new(id, NULL, 0, 0, false);
}


token_t* token_from(token_t* token, token_id id)
{
  // ignore the newline from the original token
  return token_new(id, token->source, token->line, token->pos, false);
}


token_t* token_from_string(token_t* token, const char* id)
{
  token_t* t = token_from(token, TK_ID);
  t->string = stringtab(id);

  return t;
}


token_t* token_dup(token_t* token)
{
  token_t* t = malloc(sizeof(token_t));
  memcpy(t, token, sizeof(token_t));
  return t;
}


const char* token_string(token_t* token)
{
  switch(token->id)
  {
  case TK_NONE:
    return "-";

  case TK_STRING:
  case TK_ID:
    return token->string;

  case TK_INT:
  {
    static char buf[32];
    size_t i = token->integer;
    snprintf(buf, 32, "%zu", i);
    return buf;
  }

  case TK_FLOAT:
  {
    static char buf[32];
    snprintf(buf, 32, "%g", token->real);
    return buf;
  }

  default:
    break;
  }

  for(const lexsym_t* p = abstract; p->symbol != NULL; p++)
  {
    if(token->id == p->id)
      return p->symbol;
  }

  for(const lexsym_t* p = keywords; p->symbol != NULL; p++)
  {
    if(token->id == p->id)
      return p->symbol;
  }

  for(const lexsym_t* p = symbols1; p->symbol != NULL; p++)
  {
    if(token->id == p->id)
      return p->symbol;
  }

  for(const lexsym_t* p = symbols2; p->symbol != NULL; p++)
  {
    if(token->id == p->id)
      return p->symbol;
  }

  return "UNKNOWN";
}


double token_float(token_t* token)
{
  if(token->id == TK_FLOAT)
    return token->real;

  return 0.0;
}


size_t token_int(token_t* token)
{
  if(token->id == TK_INT)
    return token->integer;

  return 0;
}


void token_setid(token_t* token, token_id id)
{
  if(token->newline)
  {
    switch(id)
    {
      case TK_LBRACE: id = TK_LBRACE_NEW; break;
      case TK_LPAREN: id = TK_LPAREN_NEW; break;
      case TK_LSQUARE: id = TK_LSQUARE_NEW; break;
      case TK_MINUS: id = TK_MINUS_NEW; break;
      default: break;
    }
  }

  token->id = id;
}


void token_setstring(token_t* token, const char* s)
{
  token->string = stringtab(s);
}


void token_free(token_t* token)
{
  if(token != NULL)
    free(token);
}
