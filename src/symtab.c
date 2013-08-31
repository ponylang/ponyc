#include "symtab.h"
#include "hash.h"
#include <stdlib.h>
#include <string.h>

#define SYMTAB_SIZE 32
#define SYMTAB_MASK (SYMTAB_SIZE - 1)

typedef struct symbol_t
{
  const char* name;
  void* value;
  struct symbol_t* next;
} symbol_t;

struct symtab_t
{
  symbol_t* symbols[SYMTAB_SIZE];
};

static void* symbol_get( symtab_t* symtab, const char* name, uint64_t hash )
{
  symbol_t* cur = symtab->symbols[hash];

  while( cur != NULL )
  {
    if( cur->name == name )
    {
      return cur->value;
    }

    cur = cur->next;
  }

  return NULL;
}

static void symbol_free( symbol_t* sym )
{
  symbol_t* next;

  while( sym != NULL )
  {
    next = sym->next;
    free( sym );
    sym = next;
  }
}

symtab_t* symtab_new()
{
  return calloc( 1, sizeof(symtab_t) );
}

void symtab_free( symtab_t* symtab )
{
  if( symtab == NULL ) { return; }

  for( int i = 0; i < SYMTAB_SIZE; i++ )
  {
    symbol_free( symtab->symbols[i] );
  }

  free( symtab );
}

#include <stdio.h>

bool symtab_add( symtab_t* symtab, const char* name, void* value )
{
  uint64_t hash = ptrhash( name ) & SYMTAB_MASK;

  if( symbol_get( symtab, name, hash ) != NULL )
  {
    printf( "no add: %s\n", name );
    return false;
  }

  symbol_t* sym = malloc( sizeof(symbol_t) );
  sym->name = name;
  sym->value = value;
  sym->next = symtab->symbols[hash];
  symtab->symbols[hash] = sym;

  return true;
}

void* symtab_get( symtab_t* symtab, const char* name )
{
  if( symtab == NULL ) return NULL;
  return symbol_get( symtab, name, ptrhash( name ) & SYMTAB_MASK );
}

bool symtab_merge( symtab_t* dst, symtab_t* src )
{
  if( src == NULL ) { return true; }

  symbol_t* cur;

  for( int i = 0; i < SYMTAB_SIZE; i++ )
  {
    cur = src->symbols[i];

    while( cur != NULL )
    {
      if( !symtab_add( dst, cur->name, cur->value ) ) { return false; }
      cur = cur->next;
    }
  }

  return true;
}
