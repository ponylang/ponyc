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

void ast_print( ast_t* ast )
{
  /* FIX: print the ast
  keep the text of every token?
  get rid of unicode escape decoding and expect unicode in the text?
    would mean all source files must be utf-8
  get rid of other escapes as well?
    currently, everything is allowed in a string
    would make entering some escapes hard
  keep the escape stuff, and keep a separate copy of the text of each token?
    this may all be heavyweight just for ast printing
  */
}
