#ifndef AST_H
#define AST_H

#include "lexer.h"
#include <stdbool.h>

/*

PROGRAM: {PACKAGE}
symtab: path -> PACKAGE

PACKAGE: {MODULE}
data: path
symtab: ID -> TYPE | TRAIT | CLASS | ACTOR

MODULE: {USE} {TYPE | TRAIT | CLASS | ACTOR}
data: source
symtab: ID -> PACKAGE | TYPE | TRAIT | CLASS | ACTOR

USE: PATH [ID]

TYPE: ID [TYPEPARAMS] NONE [TYPES] MEMBERS
TRAIT: ID [TYPEPARAMS] cap [TYPES] MEMBERS
CLASS: ID [TYPEPARAMS] cap [TYPES] MEMBERS
ACTOR: ID [TYPEPARAMS] NONE [TYPES] MEMBERS
data: typechecking state
symtab: ID -> TYPEPARAM | FVAR | FVAL | NEW | FUN | BE

MEMBERS: {FVAR | FLET | NEW | FUN | BE}

FVAR: ID [type] [SEQ]
FLET: ID [type] [SEQ]

NEW: NONE ID [TYPEPARAMS] [PARAMS | TYPES] NONE [QUESTION] [SEQ]
BE: NONE ID [TYPEPARAMS] [PARAMS | TYPES] NONE NONE [SEQ]
FUN: cap ID [TYPEPARAMS] [PARAMS | TYPES] [type] [QUESTION] [SEQ]
symtab: ID -> TYPEPARAM | PARAM

TYPEPARAMS: {TYPEPARAM}

TYPEPARAM: ID [type] [type]

TYPES: {type}

type: (UNIONTYPE | ISECTTYPE | TUPLETYPE | NOMINAL | STRUCTURAL | THISTYPE |
  ARROW)
cap: (ISO | TRN | REF | VAL | BOX | TAG | NONE)

ARROW: type type
UNIONTYPE: type type
ISECTTYPE: type type
TUPLETYPE: type type

NOMINAL: [ID] ID [TYPEARGS] cap [HAT]
data: nominal_def, if it has been resolved

STRUCTURAL: MEMBERS cap [HAT]
symtab: ID -> NEW | FUN | BE

FUNTYPE: [TYPEPARAMS] [TYPES] type

TYPEARGS: {type}

PARAMS: {PARAM}

PARAM: ID [type] [SEQ]

IDSEQ: {ID}

SEQ: {expr}
symtab: ID -> VAR | VAL

RAWSEQ: {expr}

expr
----
data: can error or not

term: local | prefix | postfix | control | infix

CONTINUE

ERROR

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
IS term term
ISNT term term
AND term term
XOR term term
OR term term
ASSIGN term term

local
-----
VAR: IDSEQ [type]
LET: IDSEQ [type]

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
CALL postfix [POSITIONALARGS] [NAMEDARGS]

control
-------
IF: RAWSEQ SEQ [SEQ]
symtab: ID -> VAR | VAL

MATCH: RAWSEQ CASES [SEQ]

CASES: {CASE}

CASE: [SEQ] [AS] [SEQ] [SEQ]
symtab: ID -> VAR | VAL

AS: IDSEQ type

WHILE: RAWSEQ SEQ [SEQ]
symtab: ID -> VAR | VAL

REPEAT: RAWSEQ SEQ
symtab: ID -> VAR | VAL

FOR: IDSEQ [type] SEQ SEQ [SEQ]

TRY: SEQ [SEQ] [SEQ]

atom
----
TUPLE: [POSITIONALARGS] [NAMEDARGS]

ARRAY: [POSITIONALARGS] [NAMEDARGS]

OBJECT: [TYPES] MEMBERS

POSITIONALARGS: {SEQ}

NAMEDARGS: {NAMEDARG}

NAMEDARG: term SEQ

THIS
ID

INT
FLOAT
STRING

ast type
--------
NEW, BE, FUN
  when expecting a CALL
  change this?

type

*/

typedef enum
{
  AST_OK,
  AST_ERROR,
  AST_FATAL
} ast_result_t;

typedef enum
{
  AST_LEFT,
  AST_RIGHT
} ast_dir_t;

typedef struct ast_t ast_t;

ast_t* ast_new(token_t* t, token_id id);
ast_t* ast_blank(token_id id);
ast_t* ast_token(token_t* t);
ast_t* ast_from(ast_t* ast, token_id id);
ast_t* ast_from_string(ast_t* ast, const char* id);
ast_t* ast_hygienic_id(ast_t* ast);
ast_t* ast_dup(ast_t* ast);
void ast_scope(ast_t* ast);
void ast_setid(ast_t* ast, token_id id);

token_id ast_id(ast_t* ast);
size_t ast_line(ast_t* ast);
size_t ast_pos(ast_t* ast);
void* ast_data(ast_t* ast);
void ast_setdata(ast_t* ast, void* data);
bool ast_canerror(ast_t* ast);
void ast_seterror(ast_t* ast);
void ast_inheriterror(ast_t* ast);
const char* ast_name(ast_t* ast);
double ast_float(ast_t* ast);
size_t ast_int(ast_t* ast);
ast_t* ast_type(ast_t* ast);
void ast_settype(ast_t* ast, ast_t* type);

ast_t* ast_nearest(ast_t* ast, token_id id);
ast_t* ast_enclosing_type(ast_t* ast);
ast_t* ast_enclosing_method(ast_t* ast);
ast_t* ast_enclosing_method_type(ast_t* ast);
ast_t* ast_enclosing_method_body(ast_t* ast);
ast_t* ast_enclosing_loop(ast_t* ast);

ast_t* ast_parent(ast_t* ast);
ast_t* ast_child(ast_t* ast);
ast_t* ast_childidx(ast_t* ast, size_t idx);
ast_t* ast_childlast(ast_t* ast);
ast_t* ast_sibling(ast_t* ast);
ast_t* ast_previous(ast_t* ast);
size_t ast_index(ast_t* ast);

void* ast_get(ast_t* ast, const char* name);
bool ast_set(ast_t* ast, const char* name, void* value);
bool ast_merge(ast_t* dst, ast_t* src);
void ast_clear(ast_t* ast);

ast_t* ast_add(ast_t* parent, ast_t* child);
ast_t* ast_pop(ast_t* ast);
ast_t* ast_append(ast_t* parent, ast_t* child);
void ast_swap(ast_t* prev, ast_t* next);
void ast_replace(ast_t** prev, ast_t* next);
void ast_reverse(ast_t* ast);
void ast_print(ast_t* ast, size_t width);
void ast_free(ast_t* ast);
void ast_free_unattached(ast_t* ast);

void ast_error(ast_t* ast, const char* fmt, ...)
  __attribute__ ((format (printf, 2, 3)));

typedef ast_result_t (*ast_visit_t)(ast_t* ast, int verbose);

ast_result_t ast_visit(ast_t* ast, ast_visit_t pre, ast_visit_t post,
  ast_dir_t dir, int verbose);

#endif
