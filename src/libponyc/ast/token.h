#ifndef TOKEN_H
#define TOKEN_H

#include "error.h"
#include "source.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct token_t token_t;

typedef enum token_id
{
  TK_EOF,
  TK_LEX_ERROR,
  TK_NONE,

  // Literals
  TK_STRING,
  TK_INT,
  TK_FLOAT,
  TK_ID,

  // Symbols
  TK_LBRACE,
  TK_RBRACE,
  TK_LPAREN,
  TK_RPAREN,
  TK_LSQUARE,
  TK_RSQUARE,

  TK_COMMA,
  TK_ARROW,
  TK_DBLARROW,
  TK_DOT,
  TK_BANG,
  TK_COLON,
  TK_SEMI,
  TK_ASSIGN,

  TK_PLUS,
  TK_MINUS,
  TK_MULTIPLY,
  TK_DIVIDE,
  TK_MOD,
  TK_HAT,

  TK_LSHIFT,
  TK_RSHIFT,

  TK_LT,
  TK_LE,
  TK_GE,
  TK_GT,

  TK_EQ,
  TK_NE,

  TK_PIPE,
  TK_AMP,
  TK_QUESTION,
  TK_UNARY_MINUS,

  // Newline symbols, only used by lexer and parser
  TK_LPAREN_NEW,
  TK_LSQUARE_NEW,
  TK_MINUS_NEW,

  // Keywords
  TK_COMPILER_INTRINSIC,

  TK_USE,
  TK_TYPE,
  TK_TRAIT,
  TK_CLASS,
  TK_ACTOR,

  TK_IS,
  TK_ISNT,

  TK_VAR,
  TK_LET,
  TK_NEW,
  TK_FUN,
  TK_BE,

  TK_ISO,
  TK_TRN,
  TK_REF,
  TK_VAL,
  TK_BOX,
  TK_TAG,

  TK_THIS,
  TK_RETURN,
  TK_BREAK,
  TK_CONTINUE,
  TK_CONSUME,
  TK_RECOVER,

  TK_IF,
  TK_THEN,
  TK_ELSE,
  TK_ELSEIF,
  TK_END,
  TK_WHILE,
  TK_DO,
  TK_REPEAT,
  TK_UNTIL,
  TK_FOR,
  TK_IN,
  TK_MATCH,
  TK_AS,
  TK_WHERE,
  TK_TRY,
  TK_ERROR,

  TK_NOT,
  TK_AND,
  TK_OR,
  TK_XOR,

  // Abstract tokens which don't directly appear in the source
  TK_PROGRAM,
  TK_PACKAGE,
  TK_MODULE,

  TK_MEMBERS,
  TK_FVAR,
  TK_FLET,

  TK_TYPES,
  TK_UNIONTYPE,
  TK_ISECTTYPE,
  TK_TUPLETYPE,
  TK_NOMINAL,
  TK_STRUCTURAL,
  TK_THISTYPE,
  TK_FUNTYPE,

  TK_TYPEPARAMS,
  TK_TYPEPARAM,
  TK_PARAMS,
  TK_PARAM,
  TK_TYPEARGS,
  TK_POSITIONALARGS,
  TK_NAMEDARGS,
  TK_NAMEDARG,

  TK_SEQ,
  TK_IDSEQ,
  TK_QUALIFY,
  TK_CALL,
  TK_TUPLE,
  TK_ARRAY,
  TK_OBJECT,
  TK_CASES,
  TK_CASE,

  TK_REFERENCE,
  TK_PACKAGEREF,
  TK_TYPEREF,
  TK_TYPEPARAMREF,
  TK_NEWREF,
  TK_BEREF,
  TK_FUNREF,
  TK_FVARREF,
  TK_FLETREF,
  TK_VARREF,
  TK_LETREF,
  TK_PARAMREF
} token_id;


/** Create a new token.
 * The created token must be freed later with token_free().
 */
token_t* token_new(token_id id, source_t* source);

/** Create a duplicate of the given token.
 * The duplicate must be freed later with token_free().
 */
token_t* token_dup(token_t* token);

/** Create a duplicate of the given token and set the ID.
 * The duplicate must be freed later with token_free().
 * This is equivalent to calling token_dup() followed by token_set_id().
 */
token_t* token_dup_new_id(token_t* token, token_id id);

/** Free the given token.
 * The token argument may be NULL.
 */
void token_free(token_t* token);


// Read accessors

/// Report the given token's ID
token_id token_get_id(token_t* token);

/** Report the given token's literal value.
 * Only valid for TK_STRING and TK_ID tokens.
 * The returned string must not be deleted and is valid for the lifetime of the
 * token.
 */
const char* token_string(token_t* token);

/// Report the given token's literal value. Only valid for TK_FLOAT tokens.
double token_float(token_t* token);

/// Report the given token's literal value. Only valid for TK_INT tokens.
__uint128_t token_int(token_t* token);

/** Return a string for printing the given token.
* The returned string must not be deleted and is only valid until the next call
* to token_print().
*/
const char* token_print(token_t* token);

/// Report the source the given token was loaded from
source_t* token_source(token_t* token);

// Report the line number the given token was found at
int token_line_number(token_t* token);

// Report the position within the line that the given token was found at
int token_line_position(token_t* token);


// Write accessors

/** Set the ID of the given token to the specified value.
 * Must not used to change tokens to TK_ID or from TK_INT or TK_FLOAT.
 */
void token_set_id(token_t* token, token_id id);

/** Set the given token's literal value.
 * Only valid for TK_STRING and TK_ID tokens.
 * The given string will be interned and hence only needs to be valid for the
 * duration of this call.
 */
void token_set_string(token_t* token, const char* value);

/// Set the given token's literal value. Only valid for TK_FLOAT tokens.
void token_set_float(token_t* token, double value);

/// Set the given token's literal value. Only valid for TK_INT tokens.
void token_set_int(token_t* token, __uint128_t value);

/// Set the given token's position within its source file
void token_set_pos(token_t* token, int line, int pos);


#endif
