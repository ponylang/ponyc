#ifndef AST_H
#define AST_H

#include "lexer.h"
#include <stdbool.h>

/*
program
  symtab: path -> package ast
  child: package
  sibling: n/a

package
  symtab: ID -> type|class ast
  child 1: path
  child 2+: module
  sibling: package

module
  symtab: ID -> package ast
  child: use|alias|class
  sibling: module

use
  symtab: n/a
  child 1: path
  child 2: ID
  sibling: use|alias|class

alias
  symtab: n/a
  child 1: ID
  child 2: type
  sibling: use|alias|class

class
  symtab: ID -> field|function ast
  child 1: ID
  child 2: formalparams
  child 3: mode
  child 4: encapsulation
  child 5: is
  child 6+: field|function // move to symtab?
  sibling: use|alias|class

field
  symtab: n/a
  child 1: ID
  child 2: type
  sibling: field|function

function
  symtab: ID -> type expression // parameters
  child 1: mode
  child 2: ID
  child 3: formalparams
  child 4: params
  child 5: type
  child 6: encapsulation
  child 7: seq
  sibling: field|function

type
is
formalparams
params
mode
encapsulation
seq
*/

typedef struct ast_t ast_t;

ast_t* ast_new( token_id id, size_t line, size_t pos, void* data );
ast_t* ast_newid( token_id id );
ast_t* ast_token( token_t* t );

token_id ast_id( ast_t* ast );
size_t ast_line( ast_t* ast );
size_t ast_pos( ast_t* ast );
void* ast_data( ast_t* ast );
const char* ast_name( ast_t* ast );

ast_t* ast_parent( ast_t* ast );
ast_t* ast_child( ast_t* ast );
ast_t* ast_sibling( ast_t* ast );

void* ast_get( ast_t* ast, const char* name );
bool ast_set( ast_t* ast, const char* name, void* value );

void ast_add( ast_t* parent, ast_t* child );
void ast_reverse( ast_t* ast );
void ast_print( ast_t* ast );
void ast_free( ast_t* ast );

#endif
