#include "ast.h"
#include "symtab.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

struct ast_t
{
  token_t* t;
  symtab_t* symtab;
  void* data;

  struct ast_t* parent;
  struct ast_t* child;
  struct ast_t* sibling;
};

void print( ast_t* ast, size_t indent );

size_t length( ast_t* ast, size_t indent )
{
  size_t len = (indent * 2) + strlen( token_string( ast->t ) );
  ast_t* child = ast->child;

  if( child != NULL ) { len += 2; }

  while( child != NULL )
  {
    len += 1 + length( child, indent );
    child = child->sibling;
  }

  return len;
}

void print_compact( ast_t* ast, size_t indent )
{
  for( size_t i = 0; i < indent; i++ ) { printf( "  " ); }
  ast_t* child = ast->child;
  bool parens = child != NULL;

  if( parens ) { printf( "(" ); }
  printf( "%s", token_string( ast->t ) );

  while( child != NULL )
  {
    printf( " " );
    print_compact( child, 0 );
    child = child->sibling;
  }

  if( parens ) { printf( ")" ); }
}

void print_extended( ast_t* ast, size_t indent )
{
  for( size_t i = 0; i < indent; i++ ) { printf( "  " ); }
  ast_t* child = ast->child;
  bool parens = child != NULL;

  if( parens ) { printf( "(" ); }
  printf( "%s\n", token_string( ast->t ) );

  while( child != NULL )
  {
    print( child, indent + 1 );
    child = child->sibling;
  }

  if( parens )
  {
    for( size_t i = 0; i <= indent; i++ ) { printf( "  " ); }
    printf( ")" );
  }
}

void print( ast_t* ast, size_t indent )
{
  if( length( ast, indent ) <= 80 )
  {
    print_compact( ast, indent );
  } else {
    print_extended( ast, indent );
  }

  printf( "\n" );
}

ast_t* ast_new( token_id id, size_t line, size_t pos, void* data )
{
  ast_t* ast = ast_token( token_new( id, line, pos ) );
  ast->data = data;
  return ast;
}

ast_t* ast_newid( token_id id )
{
  return ast_token( token_new( id, 0, 0 ) );
}

ast_t* ast_token( token_t* t )
{
  ast_t* ast = calloc( 1, sizeof(ast_t) );
  ast->t = t;

  return ast;
}

token_id ast_id( ast_t* ast )
{
  return ast->t->id;
}

size_t ast_line( ast_t* ast )
{
  return ast->t->line;
}

size_t ast_pos( ast_t* ast )
{
  return ast->t->pos;
}

void* ast_data( ast_t* ast )
{
  return ast->data;
}

const char* ast_name( ast_t* ast )
{
  switch( ast->t->id )
  {
    case TK_ID:
    case TK_STRING:
      return ast->t->string;

    default:
      assert( 0 );
      return NULL;
  }
}

ast_t* ast_parent( ast_t* ast )
{
  return ast->parent;
}

ast_t* ast_child( ast_t* ast )
{
  return ast->child;
}

ast_t* ast_sibling( ast_t* ast )
{
  return ast->sibling;
}

void* ast_get( ast_t* ast, const char* name )
{
  void* value;

  do
  {
    value = symtab_get( ast->symtab, name );
    ast = ast->parent;
  } while( (value == NULL) && (ast != NULL) );

  return value;
}

bool ast_set( ast_t* ast, const char* name, void* value )
{
  if( ast->symtab == NULL )
  {
    ast->symtab = symtab_new();
  }

  return symtab_add( ast->symtab, name, value );
}

void ast_add( ast_t* parent, ast_t* child )
{
  child->parent = parent;
  child->sibling = parent->child;
  parent->child = child;
}

void ast_reverse( ast_t* ast )
{
  if( ast == NULL ) { return; }

  ast_t* cur = ast->child;
  ast_t* next;
  ast_t* last = NULL;

  while( cur != NULL )
  {
    ast_reverse( cur );
    next = cur->sibling;
    cur->sibling = last;
    last = cur;
    cur = next;
  }

  ast->child = last;
}

void ast_print( ast_t* ast )
{
  print( ast, 0 );
  printf( "\n" );
}

void ast_free( ast_t* ast )
{
  if( ast == NULL ) { return; }

  ast_t* child = ast->child;
  ast_t* next;

  while( child != NULL )
  {
    next = child->sibling;
    ast_free( child );
    child = next;
  }

  token_free( ast->t );
  symtab_free( ast->symtab );
}
