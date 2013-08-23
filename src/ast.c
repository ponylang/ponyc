#include "ast.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

ast_t* ast_new( token_id id, size_t line, size_t pos )
{
  ast_t* ast = calloc( 1, sizeof(ast_t) );
  ast->t = calloc( 1, sizeof(token_t) );
  ast->t->id = id;
  ast->t->line = line;
  ast->t->pos = pos;

  return ast;
}

ast_t* ast_token( token_t* t )
{
  ast_t* ast = calloc( 1, sizeof(ast_t) );
  ast->t = t;

  return ast;
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
  if( ast->t != NULL ) { token_free( ast->t ); }

  ast_free( ast->child );
  ast_free( ast->sibling );
}
