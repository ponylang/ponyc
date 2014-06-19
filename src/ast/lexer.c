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

  char* buffer;
  size_t buflen;
  size_t alloc;
};

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
  { "[", TK_LBRACKET },
  { "]", TK_RBRACKET },
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
  { "[", TK_LBRACKET_NEW },
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
  { "typedef", TK_TYPEDEF },

  { "members", TK_MEMBERS },
  { "fvar", TK_FVAR },
  { "flet", TK_FLET },

  { "types", TK_TYPES },
  { "uniontype", TK_UNIONTYPE },
  { "isecttype", TK_ISECTTYPE },
  { "tupletype", TK_TUPLETYPE },
  { "nominal", TK_NOMINAL },
  { "structural", TK_STRUCTURAL },

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

  { NULL, 0 }
};

static void lexerror(lexer_t* lexer, const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  errorv(lexer->source, lexer->line, lexer->pos, fmt, ap);
  va_end(ap);
}

static bool issymbol(char c)
{
  return ((c >= '!') && (c <= '.'))
    || ((c >= ':') && (c <= '@'))
    || ((c >= '[') && (c <= '^'))
    || ((c >= '{') && (c <= '~'));
}

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
  assert(lexer->len > 0);
  return lexer->source->m[lexer->ptr];
}

static char look2(lexer_t* lexer)
{
  assert(lexer->len > 1);
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
    size_t nlen = (lexer->alloc > 0) ? lexer->alloc << 1 : 64;
    char* n = malloc(nlen);
    memcpy(n, lexer->buffer, lexer->alloc);
    free(lexer->buffer);
    lexer->buffer = n;
    lexer->alloc = nlen;
  }

  lexer->buffer[lexer->buflen] = c;
  lexer->buflen++;
}

static bool appendn(lexer_t* lexer, size_t len)
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
      c += m[i] - 'a';
    } else if((m[i] >= 'A') && (m[i] <= 'F')) {
      c += m[i] - 'a';
    } else {
      lexerror(lexer, "Escape sequence contains non-hexadecimal %c", c);
      return false;
    }
  }

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
 * A line is empty if, after whitespace, there is a \0, \n or \r\n.
 */
static bool is_empty(const char* p)
{
  return (p[0] == '\0') || (p[0] == '\n') || ((p[0] == '\r') && (p[1] == '\n'));
}

/**
 * Removes longest common prefix indentation from every line in a triple
 * quoted string. Lines containing only whitespace are removed (other than the
 * new line). If the string begins with an empty line, that line is removed
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
  char* start = NULL;
  size_t ws = 0;

  while(true)
  {
    size_t len = strspn(buf, " \t");

    if(!is_empty(buf + len))
    {
      if(start == NULL)
      {
        start = buf;
        ws = len;
      } else {
        if(len < ws)
          ws = len;

        for(size_t i = 0; i < ws; i++)
        {
          if(start[i] != buf[i])
          {
            ws = i;
            break;
          }
        }
      }

      if(ws == 0)
        break;
    }

    if((buf = strchr(buf + len, '\n')) == NULL)
      break;

    buf++;
  }

  // trim leading whitespace on each line
  if(ws > 0)
  {
    buf = lexer->buffer;
    size_t rem = lexer->buflen;

    while(true)
    {
      size_t len = strspn(buf, " \t");

      if(is_empty(buf + len))
      {
        lexer->buflen -= len;
        memmove(&buf[0], &buf[len], rem);
      } else {
        lexer->buflen -= ws;
        memmove(&buf[0], &buf[ws], rem);
      }

      char* p = strchr(buf, '\n');

      if(p == NULL)
        break;

      p++;
      rem -= (p - buf);
      buf = p;
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

static const char* copy(lexer_t* lexer)
{
  append(lexer, '\0');
  const char* str = stringtab(lexer->buffer);
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
  t->rc = 1;

  token_setid(t, id);
  return t;
}

static token_t* token(lexer_t* lexer)
{
  token_t* t = token_new(0, lexer->source, lexer->line, lexer->pos,
    lexer->newline);

  lexer->newline = false;
  return t;
}

static void nested_comment(lexer_t* lexer)
{
  size_t depth = 1;

  while(depth > 0)
  {
    if(lexer->len <= 1)
    {
      lexerror(lexer, "Nested comment doesn't terminate");
      lexer->ptr += lexer->len;
      lexer->len = 0;
      return;
    } if(look(lexer) == '*') {
      adv(lexer, 1);

      if(look(lexer) == '/')
      {
        depth--;
      }
    } else if(look(lexer) == '/') {
      adv(lexer, 1);

      if(look(lexer) == '*')
      {
        depth++;
      }
    }

    adv(lexer, 1);
  }
}

static void line_comment(lexer_t* lexer)
{
  while((lexer->len > 0) && (look(lexer) != '\n'))
  {
    adv(lexer, 1);
  }
}

static token_t* slash(lexer_t* lexer)
{
  adv(lexer, 1);

  if(lexer->len > 0)
  {
    if(look(lexer) == '*')
    {
      adv(lexer, 1);
      nested_comment(lexer);
      return NULL;
    } else if(look(lexer) == '/') {
      adv(lexer, 1);
      line_comment(lexer);
      return NULL;
    }
  }

  token_t* t = token(lexer);
  token_setid(t, TK_DIVIDE);

  return t;
}

static token_t* triple_string(lexer_t* lexer)
{
  token_t* t = token(lexer);

  while(true)
  {
    if(lexer->len == 0)
    {
      string_terminate(lexer);
      token_free(t);
      t = NULL;
      break;
    }

    char c = look(lexer);
    adv(lexer, 1);

    if((c == '\"') && (look(lexer) == '\"') && (look2(lexer) == '\"'))
    {
      adv(lexer, 2);
      token_setid(t, TK_STRING);
      normalise_string(lexer);
      t->string = copy(lexer);
      assert(t->string != NULL);
      break;
    }

    append(lexer, c);
  }

  return t;
}

static token_t* string(lexer_t* lexer)
{
  adv(lexer, 1);
  assert(lexer->buflen == 0);

  if((look(lexer) == '\"') && (look2(lexer) == '\"'))
  {
    adv(lexer, 2);
    return triple_string(lexer);
  }

  token_t* t = token(lexer);

  while(true)
  {
    if(lexer->len == 0)
    {
      string_terminate(lexer);
      token_free(t);
      return NULL;
    } else if(look(lexer) == '\"') {
      adv(lexer, 1);
      token_setid(t, TK_STRING);
      t->string = copy(lexer);
      assert(t->string != NULL);
      return t;
    } else if(look(lexer) == '\\') {
      if(lexer->len < 2)
      {
        string_terminate(lexer);
        token_free(t);
        return NULL;
      }

      adv(lexer, 1);
      char c = look(lexer);
      adv(lexer, 1);

      switch(c)
      {
      case 'a':
        append(lexer, 0x07);
        break;

      case 'b':
        append(lexer, 0x08);
        break;

      case 'e':
        append(lexer, 0x1B);
        break;

      case 'f':
        append(lexer, 0x0C);
        break;

      case 'n':
        append(lexer, 0x0A);
        break;

      case 'r':
        append(lexer, 0x0D);
        break;

      case 't':
        append(lexer, 0x09);
        break;

      case 'v':
        append(lexer, 0x0B);
        break;

      case '\"':
        append(lexer, 0x22);
        break;

      case '\\':
        append(lexer, 0x5C);
        break;

      case '0':
        append(lexer, 0x00);
        break;

      case 'x':
        appendn(lexer, 2);
        break;

      case 'u':
        appendn(lexer, 4);
        break;

      case 'U':
        appendn(lexer, 6);
        break;

      default:
        lexerror(lexer, "Invalid escape sequence: \\%c", c);
      }
    } else {
      append(lexer, look(lexer));
      adv(lexer, 1);
    }
  }

  token_free(t);
  return NULL;
}

static bool accum(lexer_t* lexer, token_t* t, __uint128_t* v,
  char c, char from, uint32_t base, uint32_t offset)
{
  __uint128_t v1 = *v;
  __uint128_t v2 = (v1 * base) + (c - from) + offset;

  if(v2 < v1)
  {
    lexerror(lexer, "overflow in numeric literal");
    token_free(t);
    return false;
  }

  *v = v2;
  return true;
}

static token_t* real(lexer_t* lexer, token_t* t, __uint128_t v)
{
  bool isreal = false;
  int e = 0;
  char c;

  if((lexer->len > 1) && (look(lexer) == '.'))
  {
    c = look2(lexer);

    if((c >= '0') && (c <= '9'))
    {
      isreal = true;
      adv(lexer, 1);

      while(lexer->len > 0)
      {
        c = look(lexer);

        if((c >= '0') && (c <= '9'))
        {
          if(!accum(lexer, t, &v, c, '0', 10, 0))
            return NULL;

          e--;
        } else if((c == 'e') || (c == 'E')) {
          break;
        } else if(isalpha(c)) {
          lexerror(lexer, "Invalid character in real number: %c", c);
          token_free(t);
          return NULL;
        } else {
          break;
        }

        adv(lexer, 1);
      }
    }
  }

  if((lexer->len > 0) && ((look(lexer) == 'e') || (look(lexer) == 'E')))
  {
    int digits = 0;
    isreal = true;
    adv(lexer, 1);

    if(lexer->len == 0)
    {
      lexerror(lexer, "Real number doesn't terminate");
      token_free(t);
      return NULL;
    }

    c = look(lexer);
    bool neg = false;
    __uint128_t n = 0;

    if((c == '+') || (c == '-'))
    {
      adv(lexer, 1);
      neg = (c == '-');

      if(lexer->len == 0)
      {
        lexerror(lexer, "Real number doesn't terminate");
        token_free(t);
        return NULL;
      }
    }

    while(lexer->len > 0)
    {
      c = look(lexer);

      if((c >= '0') && (c <= '9'))
      {
        if(!accum(lexer, t, &n, c, '0', 10, 0))
          return NULL;

        digits++;
      } else if(isalpha(c)) {
        lexerror(lexer, "Invalid character in exponent: %c", c);
        token_free(t);
        return NULL;
      } else {
        break;
      }

      adv(lexer, 1);
    }

    if(neg)
    {
      e -= n;
    } else {
      e += n;
    }

    if(digits == 0)
    {
      lexerror(lexer, "Exponent has no digits");
      token_free(t);
      return NULL;
    }
  }

  if(isreal)
  {
    token_setid(t, TK_FLOAT);
    t->real = v * pow(10.0, e);
  } else {
    token_setid(t, TK_INT);
    t->integer = v;
  }

  return t;
}

static token_t* hexadecimal(lexer_t* lexer, token_t* t)
{
  __uint128_t v = 0;
  char c;

  while(lexer->len > 0)
  {
    c = look(lexer);

    if((c >= '0') && (c <= '9'))
    {
      if(!accum(lexer, t, &v, c, '0', 16, 0))
        return NULL;
    } else if((c >= 'a') && (c <= 'z')) {
      if(!accum(lexer, t, &v, c, 'a', 16, 10))
        return NULL;
    } else if((c >= 'A') && (c <= 'Z')) {
      if(!accum(lexer, t, &v, c, 'A', 16, 10))
        return NULL;
    } else if(isalpha(c)) {
      lexerror(lexer, "Invalid character in hexadecimal number: %c", c);
      token_free(t);
      return NULL;
    } else {
      break;
    }

    adv(lexer, 1);
  }

  token_setid(t, TK_INT);
  t->integer = v;
  return t;
}

static token_t* decimal(lexer_t* lexer, token_t* t)
{
  __uint128_t v = 0;
  char c;

  while(lexer->len > 0)
  {
    c = look(lexer);

    if((c >= '0') && (c <= '9'))
    {
      if(!accum(lexer, t, &v, c, '0', 10, 0))
        return NULL;
    } else if((c == '.') || (c == 'e') || (c == 'E')) {
      return real(lexer, t, v);
    } else if(isalnum(c)) {
      lexerror(lexer, "Invalid character in decimal number: %c", c);
      token_free(t);
      return NULL;
    } else {
      break;
    }

    adv(lexer, 1);
  }

  token_setid(t, TK_INT);
  t->integer = v;
  return t;
}

static token_t* binary(lexer_t* lexer, token_t* t)
{
  __uint128_t v = 0;
  char c;

  while(lexer->len > 0)
  {
    c = look(lexer);

    if((c >= '0') && (c <= '1'))
    {
      if(!accum(lexer, t, &v, c, '0', 2, 0))
        return NULL;
    } else if(isalnum(c)) {
      lexerror(lexer, "Invalid character in binary number: %c", c);
      token_free(t);
      return NULL;
    } else {
      break;
    }

    adv(lexer, 1);
  }

  token_setid(t, TK_INT);
  t->integer = v;
  return t;
}

static token_t* number(lexer_t* lexer)
{
  token_t* t = token(lexer);

  if(look(lexer) == '0')
  {
    adv(lexer, 1);

    if(lexer->len > 0)
    {
      char c = look(lexer);

      switch(c)
      {
      case 'x':
        adv(lexer, 1);
        return hexadecimal(lexer, t);

      case 'b':
        adv(lexer, 1);
        return binary(lexer, t);

      default:
        return decimal(lexer, t);
      }
    }
  }

  return decimal(lexer, t);
}

static void read_id(lexer_t* lexer)
{
  char c;

  while(lexer->len > 0)
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
}

static token_t* identifier(lexer_t* lexer)
{
  token_t* t = token(lexer);

  read_id(lexer);
  append(lexer, '\0');

  for(const lexsym_t* p = keywords; p->symbol != NULL; p++)
  {
    if(!strcmp(lexer->buffer, p->symbol))
    {
      token_setid(t, p->id);
      lexer->buflen = 0;
      return t;
    }
  }

  token_setid(t, TK_ID);
  t->string = copy(lexer);
  return t;
}

static token_t* symbol(lexer_t* lexer)
{
  token_t* t = token(lexer);
  char sym[2];

  sym[0] = look(lexer);
  adv(lexer, 1);

  if(lexer->len > 1)
  {
    sym[1] = look(lexer);

    if(issymbol(sym[1]))
    {
      for(const lexsym_t* p = symbols2; p->symbol != NULL; p++)
      {
        if((sym[0] == p->symbol[0]) && (sym[1] == p->symbol[1]))
        {
          adv(lexer, 1);
          token_setid(t, p->id);
          return t;
        }
      }
    }
  }

  for(const lexsym_t* p = symbols1; p->symbol != NULL; p++)
  {
    if(sym[0] == p->symbol[0])
    {
      token_setid(t, p->id);
      return t;
    }
  }

  lexerror(lexer, "Unknown symbol: %c", sym[0]);
  token_free(t);
  return NULL;
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

  while((t == NULL) && (lexer->len > 0))
  {
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
      } else if(issymbol(c)) {
        t = symbol(lexer);
      } else {
        lexerror(lexer, "Unrecognized character: %c", c);
        adv(lexer, 1);
      }
    }
  }

  if(t == NULL)
  {
    t = token(lexer);
    token_setid(t, TK_EOF);
  }

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
  token->rc++;
  return token;
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

  default: {}
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
      case TK_LBRACKET: id = TK_LBRACKET_NEW; break;
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
  if(token == NULL) { return; }

  assert(token->rc > 0);
  token->rc--;

  if(token->rc == 0)
    free(token);
}
