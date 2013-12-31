#include "symtab.h"
#include "hash.h"
#include "list.h"
#include <stdlib.h>
#include <string.h>

#define SYMTAB_SIZE 32
#define SYMTAB_MASK (SYMTAB_SIZE - 1)

typedef struct symbol_t
{
  const char* name;
  void* value;
} symbol_t;

struct symtab_t
{
  list_t* symbols[SYMTAB_SIZE];
};

static void* symbol_get( symtab_t* symtab, const char* name, uint64_t hash )
{
  list_t* list = symtab->symbols[hash];

  while( list != NULL )
  {
    symbol_t* sym = list_data( list );
    if( sym->name == name ) return sym->value;
    list = list_next( list );
  }

  return NULL;
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
    list_free( symtab->symbols[i], free );
  }

  free( symtab );
}

bool symtab_add( symtab_t* symtab, const char* name, void* value )
{
  uint64_t hash = ptrhash( name ) & SYMTAB_MASK;
  if( symbol_get( symtab, name, hash ) != NULL ) return false;

  symbol_t* sym = malloc( sizeof(symbol_t) );
  sym->name = name;
  sym->value = value;
  symtab->symbols[hash] = list_push( symtab->symbols[hash], sym );

  return true;
}

void* symtab_get( symtab_t* symtab, const char* name )
{
  if( symtab == NULL ) return NULL;
  return symbol_get( symtab, name, ptrhash( name ) & SYMTAB_MASK );
}

bool symtab_merge( symtab_t* dst, symtab_t* src )
{
  if( (dst == NULL) || (src == NULL) ) { return false; }

  for( int i = 0; i < SYMTAB_SIZE; i++ )
  {
    list_t* list = src->symbols[i];

    while( list != NULL )
    {
      symbol_t* sym = list_data( list );
      if( !symtab_add( dst, sym->name, sym->value ) ) { return false; }
      list = list_next( list );
    }
  }

  return true;
}
