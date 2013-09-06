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
  child: module*

module
  data: source
  symtab: ID -> package|alias|class
  child: use|alias|class*

use
  data: n/a
  symtab: n/a
  child:
    path
    ID|NONE

alias
  data: ?
  symtab: ID -> typeparam
  child:
    ID
    list of typeparam
    type

class: trait|class|actor
  data: ?
  symtab: ID -> typeparam|param|field|function
  child:
    ID
    list of typeparam
    mode
    PRIVATE|INFER|NONE
    is: type*
    field|function*

field: var|val
  data: type_t
  symtab: n/a
  child:
    ID
    type

function: fun|msg
  data: ?
  symtab: ID -> typeparam|param
  child:
    PRIVATE|NONE
    THROW|NONE
    ID
    list of typeparam
    mode
    list of param
    type
    seq

typeparam|param
  data: type_t
  symtab: n/a
  child:
    ID
    type
    expr|NONE

type:
  adt|funtype|objtype|INFER

adt:
  data: type_t
  symtab: n/a
  child:
    type*

funtype:
  data: type_t
  symtab: n/a
  child:
    THROW|NONE
    mode
    list of type
    type

objtype:
  data: type_t
  symtab: n/a
  child:
    ID
    ID|NONE
    list of type
    mode

mode
seq
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

void* ast_get( ast_t* ast, const char* name );
bool ast_set( ast_t* ast, const char* name, void* value );
bool ast_merge( ast_t* dst, ast_t* src );

void ast_add( ast_t* parent, ast_t* child );
void ast_reverse( ast_t* ast );
void ast_print( ast_t* ast );
void ast_free( ast_t* ast );

void ast_error( ast_t* ast, const char* fmt, ... )
  __attribute__ ((format (printf, 2, 3)));

typedef bool (*ast_visit_t)( ast_t* ast );

bool ast_visit( ast_t* ast, ast_visit_t pre, ast_visit_t post );

#endif
