#include "typechecker.h"
#include "hash.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define SYMTAB_SIZE (1 << 5)
#define SYMTAB_MASK (SYMTAB_SIZE - 1)

typedef struct symbol_t
{
  char* name;
  struct symbol_t* next;
} symbol_t;

typedef struct symtab_t
{
  symbol_t* symbols[SYMTAB_SIZE];
  struct symtab_t* parent;
} symtab_t;

static symbol_t* symbol_new( char* name )
{
  symbol_t* sym = calloc( 1, sizeof(symbol_t) );
  sym->name = strdup( name );
  return sym;
}

static void symbol_free( symbol_t* sym )
{
  symbol_t* next;

  while( sym != NULL )
  {
    next = sym->next;
    if( sym->name != NULL ) { free( sym->name ); }
    free( sym );
    sym = next;
  }
}

static symtab_t* symtab_new( symtab_t* parent )
{
  symtab_t* st = calloc( 1, sizeof(symtab_t) );
  st->parent = parent;
  return st;
}

static void symtab_free( symtab_t* st )
{
  if( st == NULL ) { return; }

  for( int i = 0; i < SYMTAB_SIZE; i++ )
  {
    symbol_free( st->symbols[i] );
  }
}

static bool addsym( errorlist_t* e, symtab_t* st, ast_t* ast, char* name )
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
        return false;
      }

      sym = sym->next;
    }

    stp = stp->parent;
  }

  sym = symbol_new( name );
  sym->next = st->symbols[h];
  st->symbols[h] = sym;

  return true;
}

static void type( errorlist_t* e, symtab_t* st, ast_t* ast )
{
  addsym( e, st, ast, ast->child[0]->t->string );
}

static void trait( errorlist_t* e, symtab_t* st, ast_t* ast )
{
  addsym( e, st, ast, ast->child[0]->t->string );
}

static void object( errorlist_t* e, symtab_t* st, ast_t* ast )
{
  addsym( e, st, ast, ast->child[0]->t->string );
}

static void actor( errorlist_t* e, symtab_t* st, ast_t* ast )
{
  addsym( e, st, ast, ast->child[0]->t->string );
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

    default: assert( 0 );
    }

    ast = ast->sibling;
  }
}

errorlist_t* typecheck( ast_t* ast )
{
  errorlist_t* e = errorlist_new();
  symtab_t* st = symtab_new( NULL );

  assert( ast->t->id == TK_MODULE );
  module( e, st, ast );

  symtab_free( st );
  return e;
}
