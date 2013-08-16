#include "ast.h"
#include <stdlib.h>

ast_t* ast_new( token_id id, size_t line, size_t pos )
{
  ast_t* ast = calloc( 1, sizeof(ast_t) );
  ast->t = calloc( 1, sizeof(token_t) );
  ast->t->id = id;
  ast->t->line = line;
  ast->t->pos = pos;

  return ast;
}

void ast_free( ast_t* ast )
{
  if( ast == NULL ) { return; }
  if( ast->t != NULL ) { token_free( ast->t ); }

  ast_free( ast->child );
  ast_free( ast->sibling );
}
