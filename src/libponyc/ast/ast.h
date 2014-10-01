#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stddef.h>
#include "token.h"
#include "symtab.h"

#include <platform.h>

PONY_EXTERN_C_BEGIN

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

PACKAGE: {MODULE}
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
  TYPE: ID [TYPEPARAMS] NONE [TYPES] MEMBERS
  PRIMITIVE: ID [TYPEPARAMS] cap [TYPES] MEMBERS
  TRAIT: ID [TYPEPARAMS] cap [TYPES] MEMBERS
  CLASS: ID [TYPEPARAMS] cap [TYPES] MEMBERS
  ACTOR: ID [TYPEPARAMS] NONE [TYPES] MEMBERS
)

MEMBERS: {FVAR | FLET | method}

FVAR: ID [type] [infix]
FLET: ID [type] [infix]

method
------
data: During traits pass, trait method body came from (NULL for none).
During codegen, the LLVMBasicBlockRef for the except_block if the function or
constructor can error out.
symtab: name -> TYPEPARAM | PARAM
Methods initially have an extra child for the => before the body child. This is
removed during the parse fix pass.
(
  NEW: NONE ID [TYPEPARAMS] [PARAMS | TYPES] NONE [QUESTION] [SEQ]
  BE: NONE ID [TYPEPARAMS] [PARAMS | TYPES] NONE NONE [SEQ]
  FUN: cap ID [TYPEPARAMS] [PARAMS | TYPES] [type] [QUESTION] [SEQ]
)

TYPEPARAMS: {TYPEPARAM}

TYPEPARAM: ID [type] [type]
The second child is the contraint type, the third is the default type.

QUESTION: no children

TYPES: {type}

type
----
(
  ARROW: type type
  UNIONTYPE: {type}
  ISECTTYPE: {type}
  TUPLETYPE: {type}
  TYPEPARAMREF: ID [cap] [HAT]
  THISTYPE: no children

  NOMINAL: [ID] ID [TYPEARGS] cap [HAT]
  data: type definition

  STRUCTURAL: MEMBERS cap [HAT]
  symtab: name -> method
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

PARAM: ID type [infix]
The sequence child is the default value tree.

IDSEQ: (ID | IDSEQ) {ID | IDSEQ}

SEQ: {jump | expr}
symtab: name -> VAR | VAL
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
  ASSIGN: expr expr
)

local
-----
(
  VAR: IDSEQ [type]
  LET: IDSEQ [type]
)

prefix
------
(
  CONSUME: expr
  RECOVER: expr
  NOT: expr
  MINUS: expr
)

postfix
-------
(
  DOT: expr (ID | INT)
  BANG: expr INT
  QUALIFY: expr TYPEARGS
  CALL: expr [POSITIONALARGS] [NAMEDARGS]

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
  data: during codegen, holds the LLVMBasicBlockRef for the init_block
  symtab: name -> VAR | VAL
  Children are (in order) condition, loop body, else body.
  The condition child does not have a symbol table.

  REPEAT: SEQ SEQ [SEQ]
  data: during codegen, holds the LLVMBasicBlockRef for the cond_block
  symtab: name -> VAR | VAL
  Children are (in order) loop body, condition.
  The loop body child does not have a symbol table.

  FOR: IDSEQ [type] SEQ SEQ [SEQ]
  Children are (in order) iterator(s), iterator type, iterator initialiser,
  loop body, else body.

  TRY: SEQ [SEQ] [SEQ]
  data: during codegen, holds the LLVMBasicBlockRef for the else_block
  the then_clause (index 2) holds the LLVMValueRef for the indirectbr
  instruction
  Children are (in order) try body, else body, then body.

  CASES: {CASE}
)

CASE: [PATTERN] [SEQ] [SEQ]
symtab: name -> VAR | VAL
Children are (in order) comparison expression, as, guard, body.

PATTERN: {PATTERN | PATTERNVAR | INT | FLOAT | STRING}

PATTERNID: IDSEQ [TYPE]

atom
----
(
  TUPLE: [POSITIONALARGS] [NAMEDARGS]
  ARRAY: [POSITIONALARGS] [NAMEDARGS]
  OBJECT: [TYPES] MEMBERS
  THIS: no children
  INT: no children
  FLOAT: no children
  STRING: no children

  ID: no children
  data: From name resolution contains reference to target definition.
  During codegen, holds the LLVMValueRef for the alloca.
)

POSITIONALARGS: {SEQ}
NAMEDARGS: {NAMEDARG}
NAMEDARG: term SEQ

*/

typedef enum
{
  AST_OK,
  AST_ERROR,
  AST_FATAL
} ast_result_t;

typedef enum
{
  AST_STATE_INITIAL = 0,
  AST_STATE_INPROGRESS,
  AST_STATE_DONE
} ast_state_t;

typedef struct ast_t ast_t;

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
void ast_setid(ast_t* ast, token_id id);

token_id ast_id(ast_t* ast);
size_t ast_line(ast_t* ast);
size_t ast_pos(ast_t* ast);
void* ast_data(ast_t* ast);
void ast_setdata(ast_t* ast, void* data);
bool ast_canerror(ast_t* ast);
void ast_seterror(ast_t* ast);
void ast_inheriterror(ast_t* ast);
const char* ast_get_print(ast_t* ast);
const char* ast_name(ast_t* ast);
void ast_set_name(ast_t* ast, const char* name);
double ast_float(ast_t* ast);
size_t ast_int(ast_t* ast);
ast_t* ast_type(ast_t* ast);
void ast_settype(ast_t* ast, ast_t* type);
void ast_erase(ast_t* ast);

ast_t* ast_nearest(ast_t* ast, token_id id);
ast_t* ast_enclosing_type(ast_t* ast);
ast_t* ast_enclosing_method(ast_t* ast);
ast_t* ast_enclosing_method_type(ast_t* ast);
ast_t* ast_enclosing_method_body(ast_t* ast);
ast_t* ast_enclosing_loop(ast_t* ast);
ast_t* ast_enclosing_try(ast_t* ast, size_t* clause);
ast_t* ast_enclosing_pattern(ast_t* ast);
ast_t* ast_enclosing_constraint(ast_t* ast);

ast_t* ast_parent(ast_t* ast);
ast_t* ast_child(ast_t* ast);
ast_t* ast_childidx(ast_t* ast, size_t idx);
ast_t* ast_childlast(ast_t* ast);
size_t ast_childcount(ast_t* ast);
ast_t* ast_sibling(ast_t* ast);
ast_t* ast_previous(ast_t* ast);
size_t ast_index(ast_t* ast);

ast_t* ast_get(ast_t* ast, const char* name, sym_status_t* status);
bool ast_set(ast_t* ast, const char* name, ast_t* value, sym_status_t status);
void ast_setstatus(ast_t* ast, const char* name, sym_status_t status);
void ast_inheritstatus(ast_t* dst, ast_t* src);
void ast_inheritbranch(ast_t* dst, ast_t* src);
void ast_consolidate_branches(ast_t* ast, size_t count);
bool ast_merge(ast_t* dst, ast_t* src);
bool ast_within_scope(ast_t* outer, ast_t* inner, const char* name);
void ast_clear(ast_t* ast);

ast_t* ast_add(ast_t* parent, ast_t* child);
ast_t* ast_add_sibling(ast_t* older_sibling, ast_t* new_sibling);
ast_t* ast_pop(ast_t* ast);
ast_t* ast_append(ast_t* parent, ast_t* child);
void ast_remove(ast_t* ast, ast_t* previous_sibling);
void ast_swap(ast_t* prev, ast_t* next);
void ast_replace(ast_t** prev, ast_t* next);
void ast_free(ast_t* ast);
void ast_free_unattached(ast_t* ast);

void ast_print(ast_t* ast);
void ast_setwidth(size_t w);

void ast_error(ast_t* ast, const char* fmt, ...)
  __attribute__((format(printf, 2, 3)));

typedef struct pass_opt_t pass_opt_t;
typedef ast_result_t (*ast_visit_t)(ast_t** astp, pass_opt_t* options);

ast_result_t ast_visit(ast_t** ast, ast_visit_t pre, ast_visit_t post,
  pass_opt_t* options);

// Foreach macro, will apply macro M to each of up to 16 other arguments
#define FOREACH(M, ...) \
  EXPAND(FE(__VA_ARGS__, M, M, M, M, M, M, M, M, M, M, M, M, M, M, M, M, \
    nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop))
#define FE( \
  A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, \
  M0, M1, M2, M3, M4, M5, M6, M7, M8, M9, M10, M11, M12, M13, M14, M15, ...) \
  M0(A0) M1(A1) M2(A2) M3(A3) M4(A4) M5(A5) M6(A6) \
  M7(A7) M8(A8) M9(A9) M10(A10) M11(A11) M12(A12) M13(A13) M14(A14) M15(A15)
#define nop(x)
// We need this, because MSVC/MSVC++ treats __VA_ARGS__ as single argument if passed
// to another macro. LOL.
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


// Macros for building ASTs

/** The macros below allow for building arbitrarily complex ASTs with a simple,
 * S-expression like syntax.
 *
 * At the tope level there must be exactly one of:
 * BUILD      builds an AST and creates a variable in which to store it.
 * REPLACE    builds an AST with which it replaces the specified existing tree.
 *
 * Within these macros the following are used to build up the tree:
 * NODE       creates a node with a specified token ID and optionally children.
 * TREE       inserts an already built subtree.
 * ID         adds a TK_ID node with the given ID name.
 * NONE       is syntactic sugar to add a TK_NONE node.
 * AST_SCOPE  adds a symbol table to the enclosing node.
 */

/** Builds an AST to replace the specified existing tree.
 * A variable with the name given by var is defined and the created tree is
 * stored in it.
 * An existing node must be provided in existing, all created nodes are based
 * on this.
 */
#define BUILD(var, existing, ...) \
  ast_t* var; \
  { \
    ast_t* basis_ast = existing; \
    ast_t* parent = NULL; \
    ast_t* last_sibling = NULL; \
    ast_t* node = NULL; \
    __VA_ARGS__ \
    var = parent; \
  }

/** Builds an AST to replace the specified existing tree.
 * The provided existing must be an ast_t**.
 */
#define REPLACE(existing, ...) \
  { \
    ast_t* basis_ast = *existing; \
    ast_t* parent = NULL; \
    ast_t* last_sibling = NULL; \
    ast_t* node = NULL; \
    __VA_ARGS__ \
    ast_replace(existing, parent); \
  }


/** Add an existing subtree.
 * If the given tree is already part of another tree it will be copied
 * automatically. If it is a complete tree it will not.
 */
#define TREE(tree) \
  { \
    if(parent == NULL) parent = tree; \
    else if(last_sibling == NULL) last_sibling = ast_add(parent, tree); \
    else last_sibling = ast_add_sibling(last_sibling, tree); \
  }

/// Add a new node with the specified token ID and optionally children
#define NODE(id, ...) \
  { \
    node = ast_from(basis_ast, id); \
    TREE(node); \
    { \
      ast_t* parent = node; \
      ast_t* last_sibling = NULL; \
      ast_t* node = NULL; \
      __VA_ARGS__ \
      (void)parent; \
      (void)last_sibling; \
      (void)node; \
    } \
  }

/// Add a TK_NONE node with no children
#define NONE TREE(ast_from(basis_ast, TK_NONE));

/// Add a TK_ID node with the given ID name
#define ID(name) TREE(ast_from_string(basis_ast, name));

/// Add a TK_INT node with the given integer value
#define INT(value) TREE(ast_from_int(basis_ast, value));

/// Add a symbol table to the enclosing node
#define AST_SCOPE ast_scope(parent);

PONY_EXTERN_C_END

#endif
