#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stddef.h>
#include "token.h"
#include "symtab.h"
#include "source.h"
#include "stringtab.h"

#include <platform.h>

#if defined(PLATFORM_IS_POSIX_BASED) && defined(__cplusplus)
extern "C" {
#endif

/* Description of AST forms produced after parse fix pass.

We define what each type of node should contain. For convenience we also define
categories of node types. Node types are all capitals and correspond to the
token_id of the same name but starting with TK_. Categories are all lower case.

Node definitions look like this:

NODE_TYPE: child node types in order
data: contents of data pointer (not used if omitted)
symtab: mapping stored in symbol table (not used if omitted)
Notes (optional)

Category definitions list the node type or sub-categories they contain:

category
--------
data: contents of data pointer (not used if omitted)
symtab: mapping stored in symbol table (not used if omitted)
Notes (optional)
(
  List of contained node types and sub-categories
)

Data, symtab and notes for a category apply to all contained members.
All members defined within a category are indented to aid clarity.

NONE indicates TK_NONE throughout.


PROGRAM: {PACKAGE}
data: program_t
symtab: path -> PACKAGE
Always the root node.

PACKAGE: {MODULE} [STRING]
data: package_t
symtab: name -> entity

MODULE: {USE} {entity}
data: source_t
symtab: name -> PACKAGE | entity

USE: [ID] (STRING | FFIDECL) [expr]
The string child is the URI, including optional scheme specifier.

SPECIFIER: ID TYPEARGS [POSITIONALARGS] NONE

entity
------
data: typechecking state
symtab: name -> TYPEPARAM | FVAR | FVAL | method
(
  TYPE: ID [TYPEPARAMS] cap [TYPES] NONE NONE [STRING]
  PRIMITIVE: ID [TYPEPARAMS] cap [TYPES] MEMBERS NONE [STRING]
  TRAIT: ID [TYPEPARAMS] cap [TYPES] MEMBERS NONE [STRING]
  CLASS: ID [TYPEPARAMS] cap [TYPES] MEMBERS NONE [STRING]
  ACTOR: ID [TYPEPARAMS] NONE [TYPES] MEMBERS [AT] [STRING]
)

MEMBERS: {FVAR | FLET | method}

FVAR: ID [type] [infix]
FLET: ID [type] [infix]

method
------
data: During traits pass, trait method body came from (NULL for none).
symtab: name -> TYPEPARAM | PARAM
(
  NEW: cap ID [TYPEPARAMS] [PARAMS] NONE [QUESTION] [SEQ] [STRING]
  BE: cap ID [TYPEPARAMS] [PARAMS] NONE NONE [SEQ] [STRING]
  FUN: cap ID [TYPEPARAMS] [PARAMS] [type] [QUESTION] [SEQ] [STRING]
)

TYPEPARAMS: {TYPEPARAM}

TYPEPARAM: ID [type] [type]
The second child is the contraint type, the third is the default type.

QUESTION: no children

TYPES: {type}

type
----
(
  UNIONTYPE: {type}
  data: During parse and parsefix passes indicates if union was immediately
  inside grouping parentheses in the source. 1 => in parentheses, 0 => not.

  ISECTTYPE: {type}
  data: During parse and parsefix passes indicates if intersect was immediately
  inside grouping parentheses in the source. 1 => in parentheses, 0 => not.

  ARROW: type type
  TUPLETYPE: {type}
  THISTYPE: no children

  TYPEPARAMREF: ID [cap] [EPHEMERAL | BORROWED]
  data: typeparam definition

  NOMINAL: [ID] ID [TYPEARGS] cap [EPHEMERAL | BORROWED] [ID]
  data: type definition
)

cap
---
(
  ISO: no children
  TRN: no children
  REF: no children
  VAL: no children
  BOX: no children
  TAG: no children
)

TYPEARGS: {type}

PARAMS: {PARAM}

PARAM: (ID type [infix] | ELLIPSIS)
The sequence child is the default value tree.

ELLIPSIS: no children

SEQ: {jump | expr}
symtab: name -> VAR | VAL
data:
  During parsing, whether sequence is a test scope (1) or not (0)
  During type checking, whether the expr can error or not
Some SEQ nodes do not have symbol tables. These are indicated in the parent
nodes.

jump
----
data: during type checking, whether the expr can error or not
(
  CONTINUE: no children
  ERROR: no children
  BREAK: expr
  RETURN: expr
)

expr
----
data: during type checking, whether the expr can error or not
(
  local
  prefix
  postfix
  control
  infix
  atom
)

infix
-----
data: During parse and parsefix passes infix operators other than assign use
the data field to indicate if they were immediately inside grouping parentheses
in the source. 1 => in parentheses, 0 => not.
(
  MULTIPLY: expr expr
  DIVIDE: expr expr
  MOD: expr expr
  PLUS: expr expr
  MINUS: expr expr
  LSHIFT: expr expr
  RSHIFT: expr expr
  LT: expr expr
  LE: expr expr
  GE: expr expr
  GT: expr expr
  EQ: expr expr
  NE: expr expr
  IS: expr expr
  ISNT: expr expr
  AND: expr expr
  XOR: expr expr
  OR: expr expr
  ASSIGN: expr expr // note that the LHS comes second, to handle init tracking
)

local
-----
(
  VAR: ID [type]
  LET: ID [type]
)

prefix
------
(
  CONSUME: cap expr
  RECOVER: cap expr
  NOT: expr
  MINUS: expr
)

postfix
-------
(
  DOT: expr (ID | INT)
  BANG: expr INT
  QUALIFY: expr TYPEARGS
  CALL: [POSITIONALARGS] [NAMEDARGS] expr // note the receiver comes last

  AT: ID TYPEARGS [POSITIONALARGS] NONE
  The final child is initially any named arguments. The parse fix pass gives an
  error if this is not NONE.
)

control
-------
(
  IF: SEQ SEQ [SEQ]
  symtab: name -> VAR | VAL
  Children are (in order) condition, then body, else body.
  The condition child does not have a symbol table.

  MATCH: SEQ CASES [SEQ]
  The first child is the expression to switch on and it does not have a symbol
  table.
  The final child is the else body.

  WHILE: SEQ SEQ [SEQ]
  symtab: name -> VAR | VAL
  Children are (in order) condition, loop body, else body.
  The condition child does not have a symbol table.

  REPEAT: SEQ SEQ [SEQ]
  symtab: name -> VAR | VAL
  Children are (in order) loop body, condition.
  The loop body child does not have a symbol table.

  FOR: (LET | TUPLE) SEQ SEQ [SEQ]
  Children are (in order) iterator(s), iterator initialiser, loop body,
  else body.

  TRY: SEQ [SEQ] [SEQ]
  the then_clause (index 2) holds the LLVMValueRef for the indirectbr
  instruction
  Children are (in order) try body, else body, then body.

  CASES: {CASE}
)

CASE: [INFIX] [SEQ] [SEQ]
symtab: name -> VAR | VAL
Children are (in order) pattern, guard, body.

atom
----
(
  TUPLE: {expr}
  ARRAY: [TYPE] {expr}
  OBJECT: [TYPES] MEMBERS
  THIS: no children
  INT: no children
  FLOAT: no children
  STRING: no children

  ID: no children
  data: During codegen, holds the LLVMValueRef for the alloca.
)

POSITIONALARGS: {SEQ}
NAMEDARGS: {NAMEDARG}
NAMEDARG: ID SEQ

*/

typedef struct ast_t ast_t;

typedef enum
{
  AST_OK,
  AST_IGNORE,
  AST_ERROR,
  AST_FATAL
} ast_result_t;

typedef enum
{
  AST_STATE_INITIAL = 0,
  AST_STATE_INPROGRESS,
  AST_STATE_DONE
} ast_state_t;

ast_t* ast_new(token_t* t, token_id id);
ast_t* ast_blank(token_id id);
ast_t* ast_token(token_t* t);
ast_t* ast_from(ast_t* ast, token_id id);
ast_t* ast_from_string(ast_t* ast, const char* name);
ast_t* ast_from_int(ast_t* ast, uint64_t value);
ast_t* ast_dup(ast_t* ast);
void ast_scope(ast_t* ast);
bool ast_has_scope(ast_t* ast);
symtab_t* ast_get_symtab(ast_t* ast);
ast_t* ast_setid(ast_t* ast, token_id id);
void ast_setpos(ast_t* ast, size_t line, size_t pos);
void ast_setdebug(ast_t* ast, bool state);

token_id ast_id(ast_t* ast);
size_t ast_line(ast_t* ast);
size_t ast_pos(ast_t* ast);
bool ast_debug(ast_t* ast);
source_t* ast_source(ast_t* ast);

void* ast_data(ast_t* ast);
void ast_setdata(ast_t* ast, void* data);
bool ast_canerror(ast_t* ast);
void ast_seterror(ast_t* ast);
bool ast_cansend(ast_t* ast);
void ast_setsend(ast_t* ast);
bool ast_mightsend(ast_t* ast);
void ast_setmightsend(ast_t* ast);
void ast_clearmightsend(ast_t* ast);
bool ast_inprogress(ast_t* ast);
void ast_setinprogress(ast_t* ast);
void ast_clearinprogress(ast_t* ast);
void ast_inheritflags(ast_t* ast);

const char* ast_get_print(ast_t* ast);
const char* ast_name(ast_t* ast);
void ast_set_name(ast_t* ast, const char* name);
double ast_float(ast_t* ast);
__uint128_t ast_int(ast_t* ast);
size_t ast_size_t(ast_t* ast);
ast_t* ast_type(ast_t* ast);
void ast_settype(ast_t* ast, ast_t* type);
void ast_erase(ast_t* ast);

ast_t* ast_nearest(ast_t* ast, token_id id);
ast_t* ast_try_clause(ast_t* ast, size_t* clause);

ast_t* ast_parent(ast_t* ast);
ast_t* ast_child(ast_t* ast);
ast_t* ast_childidx(ast_t* ast, size_t idx);
ast_t* ast_childlast(ast_t* ast);
size_t ast_childcount(ast_t* ast);
ast_t* ast_sibling(ast_t* ast);
ast_t* ast_previous(ast_t* ast);
size_t ast_index(ast_t* ast);

ast_t* ast_get(ast_t* ast, const char* name, sym_status_t* status);
ast_t* ast_get_case(ast_t* ast, const char* name, sym_status_t* status);
bool ast_set(ast_t* ast, const char* name, ast_t* value, sym_status_t status);
void ast_setstatus(ast_t* ast, const char* name, sym_status_t status);
void ast_inheritstatus(ast_t* dst, ast_t* src);
void ast_inheritbranch(ast_t* dst, ast_t* src);
void ast_consolidate_branches(ast_t* ast, size_t count);
bool ast_merge(ast_t* dst, ast_t* src);
bool ast_within_scope(ast_t* outer, ast_t* inner, const char* name);
bool ast_all_consumes_in_scope(ast_t* outer, ast_t* inner);
void ast_clear(ast_t* ast);
void ast_clear_local(ast_t* ast);

ast_t* ast_add(ast_t* parent, ast_t* child);
ast_t* ast_add_sibling(ast_t* older_sibling, ast_t* new_sibling);
ast_t* ast_pop(ast_t* ast);
ast_t* ast_append(ast_t* parent, ast_t* child);
void ast_remove(ast_t* ast);
void ast_swap(ast_t* prev, ast_t* next);
void ast_replace(ast_t** prev, ast_t* next);
void ast_reorder_children(ast_t* ast, const size_t* new_order,
  ast_t** shuffle_space);
void ast_free(ast_t* ast);
void ast_free_unattached(ast_t* ast);

void ast_print(ast_t* ast);
const char* ast_print_type(ast_t* type);
void ast_setwidth(size_t w);

void ast_error(ast_t* ast, const char* fmt, ...)
  __attribute__((format(printf, 2, 3)));

typedef struct pass_opt_t pass_opt_t;
typedef ast_result_t (*ast_visit_t)(ast_t** astp, pass_opt_t* options);

ast_result_t ast_visit(ast_t** ast, ast_visit_t pre, ast_visit_t post,
  pass_opt_t* options);

// Foreach macro, will apply macro M to each of up to 30 other arguments
#define FOREACH(M, ...) \
  EXPAND(FE(__VA_ARGS__, M, M, M, M, M, M, M, M, M, M, M, M, M, M, M, M, \
    M, M, M, M, M, M, M, M, M, M, M, M, M, M, NOP, NOP, NOP, NOP, NOP, NOP, \
    NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, \
    NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP))
#define FE( \
  A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, \
  A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, \
  M0, M1, M2, M3, M4, M5, M6, M7, M8, M9, M10, M11, M12, M13, M14, M15, \
  M16, M17, M18, M19, M20, M21, M22, M23, M24, M25, M26, M27, M28, M29, ...) \
  M0(A0) M1(A1) M2(A2) M3(A3) M4(A4) M5(A5) M6(A6) M7(A7) M8(A8) M9(A9) \
  M10(A10) M11(A11) M12(A12) M13(A13) M14(A14) M15(A15) M16(A16) M17(A17) \
  M18(A18) M19(A19) M20(A20) M21(A21) M22(A22) M23(A23) M24(A24) M25(A25) \
  M26(A26) M27(A27) M28(A28) M29(A29)

#define NOP(x)
// We need this, because MSVC/MSVC++ treats __VA_ARGS__ as single argument if
// passed to another macro
#define EXPAND(x) x

typedef ast_t* ast_ptr_t; // Allows easier decalaration of locals
#define ADDR_AST(x) &x,

void ast_get_children(ast_t* parent, size_t child_count,
  ast_t*** out_children);

#define AST_GET_CHILDREN(parent, ...) \
  ast_ptr_t __VA_ARGS__; \
  AST_GET_CHILDREN_NO_DECL(parent, __VA_ARGS__)

#define AST_GET_CHILDREN_NO_DECL(parent, ...) \
  { \
    ast_t** children[] = { FOREACH(ADDR_AST, __VA_ARGS__) NULL }; \
    ast_get_children(parent, (sizeof(children) / sizeof(ast_t**)) - 1, \
      children); \
  }


void ast_extract_children(ast_t* parent, size_t child_count,
  ast_t*** out_children);

#define AST_EXTRACT_CHILDREN(parent, ...) \
  ast_ptr_t __VA_ARGS__; \
  { \
    ast_t** children[] = { FOREACH(ADDR_AST, __VA_ARGS__) NULL }; \
    ast_extract_children(parent, (sizeof(children)/sizeof(ast_t**)) - 1, \
      children); \
  }


#if defined(PLATFORM_IS_POSIX_BASED) && defined(__cplusplus)
}
#endif

#endif
