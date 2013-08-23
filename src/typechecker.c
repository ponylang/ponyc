#include "typechecker.h"
#include "symtab.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#if 0
static symbol_t* addsym( errorlist_t* e, symtab_t* st, ast_t* ast, char* name, token_id id )
{
  uint64_t h = strhash( name ) & SYMTAB_MASK;
  symtab_t* stp = st;
  symbol_t* sym;

  while( stp != NULL )
  {
    symbol_t* sym = stp->symbols[h];

    while( sym != NULL )
    {
      if( !strcmp( sym->name, name ) )
      {
        error_new( e, ast->t->line, ast->t->pos, "Symbol %s was defined previously", name );
        return NULL;
      }

      sym = sym->next;
    }

    stp = stp->parent;
  }

  sym = symbol_new( name, id );
  sym->next = st->symbols[h];
  st->symbols[h] = sym;

  return sym;
}

static void type( errorlist_t* e, symtab_t* st, ast_t* ast )
{
  symbol_t* sym = addsym( e, st, ast, ast->child[0]->t->string, TK_TYPE );
  if( sym == NULL ) { return; }
}

static void trait( errorlist_t* e, symtab_t* st, ast_t* ast )
{
  symbol_t* sym = addsym( e, st, ast, ast->child[0]->t->string, TK_TRAIT );
  if( sym == NULL ) { return; }
}

static void object( errorlist_t* e, symtab_t* st, ast_t* ast )
{
  // TYPEID formalargs is typebody
  symbol_t* sym = addsym( e, st, ast, ast->child[0]->t->string, TK_OBJECT );
  if( sym == NULL ) { return; }

  sym->st = symtab_new( st );
}

static void actor( errorlist_t* e, symtab_t* st, ast_t* ast )
{
  symbol_t* sym = addsym( e, st, ast, ast->child[0]->t->string, TK_ACTOR );
  if( sym == NULL ) { return; }
}

static void module( errorlist_t* e, symtab_t* st, ast_t* ast )
{
  ast = ast->child[0];

  while( ast != NULL )
  {
    switch( ast->t->id )
    {
    case TK_USE:
      break;

    case TK_DECLARE:
      break;

    case TK_TYPE: type( e, st, ast ); break;
    case TK_TRAIT: trait( e, st, ast ); break;
    case TK_OBJECT: object( e, st, ast); break;
    case TK_ACTOR: actor( e, st, ast ); break;

    default: abort();
    }

    ast = ast->sibling;
  }
}
#endif

bool typecheck( ast_t* ast, errorlist_t* errors )
{
#if 0
  symtab_t* st = symtab_new( NULL );

  if( ast->t->id != TK_MODULE ) { abort(); }
  module( e, st, ast );

  symtab_free( st );
  return e;
#endif
  return false;
}
