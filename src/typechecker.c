#include "typechecker.h"

typedef struct typedesc_t
{
  const char* name;
  // FIX: formal parameters
  // FIX: constructors, functions, messages
} typedesc_t;

typedef struct symtab_t
{
  struct symtab_t* parent;
} symtab_t;

typedesc_t* gettype( symtab_t* st, const char* package, const char* name )
{
  // FIX:
  return NULL;
}

static void module( ast_t* ast, errorlist_t* e )
{
  if( ast->t->id != TK_MODULE )
  {
    error_new( e, 0, 0, "AST does not begin with a module" );
    return;
  }

  ast_t* a = ast->child[0];

  while( a != NULL )
  {
    switch( a->t->id )
    {
    case TK_USE:
      break;

    case TK_DECLARE:
      break;

    case TK_TYPE:
      break;

    case TK_TRAIT:
      break;

    case TK_OBJECT:
      break;

    case TK_ACTOR:
      break;

    default:
      error_new( e, 0, 0, "Unexpected AST node in module" );
    }

    a = a->sibling;
  }
}

errorlist_t* typecheck( ast_t* ast )
{
  errorlist_t* e = errorlist_new();
  module( ast, e );
  return e;
}
