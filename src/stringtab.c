#include "stringtab.h"
#include "hash.h"
#include "list.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define HASH_SIZE 4096
#define HASH_MASK (HASH_SIZE - 1)

typedef struct stringtab_t
{
  list_t* string[HASH_SIZE];
} stringtab_t;

static stringtab_t table;

const char* stringtab( const char* string )
{
  if( string == NULL ) { return NULL; }

  uint64_t hash = strhash( string, 0 ) & HASH_MASK;
  list_t* cur = table.string[hash];

  while( cur != NULL )
  {
    const char* data = list_data( cur );

    if( !strcmp( data, string ) )
    {
      return data;
    }

    cur = list_next( cur );
  }

  const char* data = strdup( string );
  table.string[hash] = list_push( table.string[hash], data );

  return data;
}

void stringtab_done()
{
  for( int i = 0; i < HASH_SIZE; i++ )
  {
    list_free( table.string[i], free );
  }

  memset( table.string, 0, HASH_SIZE * sizeof(list_t*) );
}
