#ifndef AST_H
#define AST_H

#include "lexer.h"
#include <stdbool.h>

/*
program
  symtab: path -> package
  package*

package
  data: path
  symtab: ID -> alias|class
  module*

module
  data: source
  symtab: ID -> package|alias|class
  (use|alias|class)*

use
  path
  ID|NONE

alias
  symtab: ID -> typeparam
  ID
  list of typeparam
  type

class: trait|class|actor
  symtab: ID -> typeparam|param|field|function
  ID
  list of typeparam
  mode
  (PRIVATE|INFER|NONE)
  is: type*
  list of (field|function)

field
  (VAR|VAL)
  ID
  type

function: fun|msg
  symtab: ID -> typeparam|param
  (PRIVATE|NONE)
  (THROW|NONE)
  ID
  list of typeparam
  mode
  list of param
  type
  (seq|canthrow|NONE)

typeparam|param
  ID
  type
  expr

type: adt|funtype|objtype|INFER

adt
  type*

funtype
  data: type_t
  (THROW|NONE)
  mode
  list of type
  type

objtype
  data: type_t
  ID
  (ID|NONE)
  list of type
  mode

mode
  list of (ISO|VAR|VAL|TAG|THIS|ID)
  THIS|ID|NONE

canthrow
  (expr|seq)

seq
  symtab: ID -> local
  expr*

expr
  :local|lambda
  |if|match
  |while|do|for
  |break|continue|return
  |try|throw
  |binary|equals

local
  (VAR|VAL)
  ID
  type
  expr

lambda
  THROW
  mode
  list of param
  type
  (expr|canthrow)

if
  seq
  expr
  (expr|NONE)

match
  seq
  list of case

case
  (binary|NONE)
  as: (ID, type)|NONE
  (binary|NONE)
  seq

while
  seq
  expr

do
  seq
  expr

for
  ID
  type
  seq
  expr

try
  canthrow
  (seq|NONE)
  (expr|NONE)

return
  expr

break
continue
throw

postfix: primary|dot|typeargs|call

primary: THIS|INT|FLOAT|STRING|ID|seq

dot
  postfix
  ID

typeargs
  postfix
  list of type

call
  postfix
  list of arg

unop: NOT|MINUS

unary: (unop: unary)|postfix

binop
  :AND|OR|XOR
  |PLUS|MINUS|MULTIPLY|DIVIDE|MOD
  |LSHIFT|RSHIFT
  |EQ|NEQ|LT|LE|GE|GT

binary: (binop: binary unary)|unary

equals
  binary
  expr
*/

typedef struct ast_t ast_t;

ast_t* ast_new( token_id id, size_t line, size_t pos, void* data );
ast_t* ast_newid( token_id id );
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
void ast_append( ast_t* parent, ast_t* child );
void ast_reverse( ast_t* ast );
void ast_print( ast_t* ast );
void ast_free( ast_t* ast );

void ast_error( ast_t* ast, const char* fmt, ... )
  __attribute__ ((format (printf, 2, 3)));

typedef bool (*ast_visit_t)( ast_t* ast );

bool ast_visit( ast_t* ast, ast_visit_t pre, ast_visit_t post );

#endif
