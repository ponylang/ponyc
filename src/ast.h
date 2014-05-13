#ifndef AST_H
#define AST_H

#include "lexer.h"
#include <stdbool.h>

/*

PROGRAM: {PACKAGE}
symtab: path -> PACKAGE

PACKAGE: {MODULE}
data: path
symtab: ID -> TYPEDECL | TRAIT | CLASS | ACTOR

MODULE: {USE | TYPEDECL | TRAIT | CLASS | ACTOR}
data: source
symtab: ID -> PACKAGE | TYPEDECL | TRAIT | CLASS | ACTOR

USE: PATH [ID]

TYPEDECL: ID [TYPEPARAMS] [TYPEEXPR | NOMINAL | STRUCTURAL | TYPEDECL]
symtab: ID -> TYPEPARAM

TRAIT: ID [TYPEPARAMS] [RAW_CAP] [TYPES] MEMBERS
CLASS: ID [TYPEPARAMS] [RAW_CAP] [TYPES] MEMBERS
ACTOR: ID [TYPEPARAMS] [RAW_CAP] [TYPES] MEMBERS
symtab: ID -> TYPEPARAM | VAR | VAL | NEW | FUN | BE

MEMBERS: {VAR | VAL | NEW | FUN | BE}

VAR: ID [TYPE] [SEQ]
VAL: ID [TYPE] [SEQ]

NEW: NONE ID [TYPEPARAMS] [PARAMS] NONE [QUESTION] [SEQ]
FUN: RAW_CAP ID [TYPEPARAMS] [PARAMS] [TYPE] [QUESTION] [SEQ]
BE: NONE ID [TYPEPARAMS] [PARAMS] NONE NONE [SEQ]
symtab: ID -> TYPEPARAM | PARAM

TYPEPARAMS: {TYPEPARAM}

TYPEPARAM: ID [TYPE] [SEQ]

TYPES: {TYPE}

TYPE: (UNIONTYPE | TUPLETYPE | NOMINAL | STRUCTURAL | TYPEDECL) [HAT]

UNIONTYPE: TYPE TYPE [CAP]
TUPLETYPE: TYPE TYPE [CAP]

NOMINAL: TYPENAME [TYPEARGS] [CAP]

TYPENAME: {ID}

TYPEARGS: {TYPE}

STRUCTURAL: {NEWTYPE | FUNTYPE | BETYPE} [CAP]

NEWTYPE: NONE [ID] [TYPEPARAMS] [TYPES] NONE [QUESTION]
FUNTYPE: RAW_CAP [ID] [TYPEPARAMS] [TYPES] [TYPE] [QUESTION]
BETYPE: NONE [ID] [TYPEPARAMS] [TYPES] NONE NONE

PARAMS: {PARAM}

PARAM: ID [TYPE] [SEQ]

IDSEQ: {ID}

SEQ: {expr}
symtab: ID -> VAR | VAL

expr
----
CONTINUE:

UNDEF:

BREAK: infix

RETURN: infix

infix
-----
MULTIPLY term term
DIVIDE term term
MOD term term
PLUS term term
MINUS term term
LSHIFT term term
RSHIFT term term
LT term term
LE term term
GE term term
GT term term
IS term term
EQ term term
NE term term
AND term term
XOR term term
OR term term
ASSIGN term term

term: local | prefix | postfix | control

local
-----
VAR: IDSEQ [TYPE] // ??? same names as fields - problem?
VAL: IDSEQ [TYPE]

prefix
------
CONSUME: term
RECOVER: term
NOT: term
MINUS: term

postfix
-------
atom
DOT postfix (ID | INT)
BANG postfix INT
QUALIFY postfix TYPEARGS
CALL postfix TUPLE

control
-------
IF: SEQ SEQ [SEQ]
symtab: ID -> VAR | VAL

WHILE: SEQ SEQ [SEQ]
symtab: ID -> VAR | VAL

DO: SEQ SEQ
symtab: ID -> VAR | VAL

FOR: IDSEQ [TYPE] SEQ SEQ [SEQ]

TRY: SEQ [SEQ] [SEQ]

atom
----
TUPLE: [POSITIONAL] [NAMED]

ARRAY: [POSITIONAL] [NAMED]

OBJECT: [TYPES] MEMBERS

THIS:
INT:
FLOAT:
STRING:
ID:

*/

typedef struct ast_t ast_t;

ast_t* ast_new( token_id id, size_t line, size_t pos, void* data );
ast_t* ast_token( token_t* t );
void ast_attach( ast_t* ast, void* data );
void ast_scope( ast_t* ast );

token_id ast_id( ast_t* ast );
size_t ast_line( ast_t* ast );
size_t ast_pos( ast_t* ast );
void* ast_data( ast_t* ast );
const char* ast_name( ast_t* ast );

ast_t* ast_nearest( ast_t* ast, token_id id );
ast_t* ast_parent( ast_t* ast );
ast_t* ast_child( ast_t* ast );
ast_t* ast_childidx( ast_t* ast, size_t idx );
ast_t* ast_sibling( ast_t* ast );
size_t ast_index( ast_t* ast );
size_t ast_childcount( ast_t* ast );

void* ast_get( ast_t* ast, const char* name );
bool ast_set( ast_t* ast, const char* name, void* value );
bool ast_merge( ast_t* dst, ast_t* src );

void ast_add( ast_t* parent, ast_t* child );
ast_t* ast_pop( ast_t* ast );
void ast_append( ast_t* parent, ast_t* child );
void ast_reverse( ast_t* ast );
void ast_print( ast_t* ast, size_t width );
void ast_free( ast_t* ast );

void ast_error( ast_t* ast, const char* fmt, ... )
  __attribute__ ((format (printf, 2, 3)));

typedef bool (*ast_visit_t)( ast_t* ast );

bool ast_visit( ast_t* ast, ast_visit_t pre, ast_visit_t post );

#endif
