#include "symtab.h"
#include "hash.h"
#include <stdlib.h>
#include <string.h>

#define SYMTAB_SIZE 32
#define SYMTAB_MASK (SYMTAB_SIZE - 1)

typedef struct symbol_t
{
  const char* name;
  const type_t* type;
  struct symbol_t* next;
} symbol_t;

struct symtab_t
{
  symbol_t* symbols[SYMTAB_SIZE];
  uint64_t refcount;
  struct symtab_t* parent;
};

static symbol_t* symbol( symtab_t* symtab, const char* name, uint64_t hash )
{
  if( symtab == NULL ) return NULL;
  symbol_t* cur = symtab->symbols[hash];

  while( cur != NULL )
  {
    if( !strcmp( cur->name, name ) )
    {
      return cur;
    }

    cur = cur->next;
  }

  return symbol( symtab->parent, name, hash );
}

static void symbol_free( symbol_t* sym )
{
  if( sym == NULL ) { return; }
  free( sym );
}

symtab_t* symtab_new( symtab_t* parent )
{
  if( parent != NULL ) { symtab_ref( parent ); }

  symtab_t* symtab = calloc( 1, sizeof(symtab_t) );
  symtab->refcount = 1;
  symtab->parent = parent;

  return symtab;
}

void symtab_ref( symtab_t* symtab )
{
  symtab->refcount++;
}

void symtab_free( symtab_t* symtab )
{
  if( symtab == NULL ) { return; }
  if( (--symtab->refcount) > 0 ) { return; }
  if( symtab->parent != NULL ) { symtab_free( symtab->parent ); }

  for( int i = 0; i < SYMTAB_SIZE; i++ )
  {
    symbol_free( symtab->symbols[i] );
  }

  free( symtab );
}

bool symtab_add( symtab_t* symtab, const char* name, const type_t* type )
{
  uint64_t hash = strhash( name ) & SYMTAB_MASK;

  if( symbol( symtab, name, hash ) != NULL )
  {
    return false;
  }

  symbol_t* sym = malloc( sizeof(symbol_t) );
  sym->name = name;
  sym->type = type;
  sym->next = symtab->symbols[hash];
  symtab->symbols[hash] = sym;

  return true;
}

const type_t* symtab_get( symtab_t* symtab, const char* name )
{
  uint64_t hash = strhash( name ) & SYMTAB_MASK;
  symbol_t* sym = symbol( symtab, name, hash );

  if( sym == NULL ) { return NULL; }
  return sym->type;
}
