#include "typechecker.h"
#include "hash.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#define SYMTAB_SIZE (1 << 5)
#define SYMTAB_MASK (SYMTAB_SIZE - 1)

typedef struct symbol_t symbol_t;
typedef struct symtab_t symtab_t;

struct symbol_t
{
  char* name;
  token_id id;
  symtab_t* st;

  symbol_t* next;
};

struct symtab_t
{
  symbol_t* symbols[SYMTAB_SIZE];
  symtab_t* parent;
};

// forward declarations
static void symtab_free( symtab_t* st );

static symbol_t* symbol_new( char* name, token_id id )
{
  symbol_t* sym = calloc( 1, sizeof(symbol_t) );
  sym->name = strdup( name );
  sym->id = id;

  return sym;
}

static void symbol_free( symbol_t* sym )
{
  if( sym->name != NULL ) { free( sym->name ); }
  symtab_free( sym->st );
  free( sym );
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
    symbol_t* sym = st->symbols[i];
    symbol_t* next;

    while( sym != NULL )
    {
      next = sym->next;
      symbol_free( sym );
      sym = next;
    }
  }
}

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

errorlist_t* typecheck( ast_t* ast )
{
  errorlist_t* e = errorlist_new();
  symtab_t* st = symtab_new( NULL );

  if( ast->t->id != TK_MODULE ) { abort(); }
  module( e, st, ast );

  symtab_free( st );
  return e;
}
