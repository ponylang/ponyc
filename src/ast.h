#ifndef AST_H
#define AST_H

#include "lexer.h"
#include <stdbool.h>

/*
program
  symtab: path -> package
  child: package
  sibling: n/a

package
  data: path
  symtab: ID -> alias|class
  child: module

module
  data: source
  symtab: ID -> package
  child: use|alias|class

use
  data: n/a
  symtab: n/a
  child 1: path
  child 2: ID

alias
  data: ?
  symtab: n/a
  child 1: ID
  child 2: formalparams
  child 3: type

class
  data: ?
  symtab: ID -> formal|field|function
  child 1: ID
  child 2: formalparams
  child 3: mode
  child 4: encapsulation
  child 5: is
  child 6+: field|function

field
  data: type expression ?
  symtab: n/a
  child 1: ID
  child 2: type

function
  data: type expression ?
  symtab: ID -> formal|param
  child 1: mode
  child 2: ID
  child 3: formalparams
  child 4: params
  child 5: type
  child 6: encapsulation
  child 7: seq

formalparams
params
  data: ?
  symtab: n/a
  child: (ID type|INFER expr|NONE)

type:(adttype|partialtype|functiontype|objecttype)
is
mode
encapsulation
seq
*/

typedef struct ast_t ast_t;

ast_t* ast_new( token_id id, size_t line, size_t pos, void* data );
ast_t* ast_newid( token_id id );
ast_t* ast_token( token_t* t );
void ast_attach( ast_t* ast, void* data );

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

void* ast_get( ast_t* ast, const char* name );
bool ast_set( ast_t* ast, const char* name, void* value );

void ast_add( ast_t* parent, ast_t* child );
void ast_reverse( ast_t* ast );
void ast_print( ast_t* ast );
void ast_free( ast_t* ast );

void ast_error( ast_t* ast, const char* fmt, ... )
  __attribute__ ((format (printf, 2, 3)));

#endif
