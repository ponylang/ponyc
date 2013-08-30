#include "stringtab.h"
#include "hash.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define HASH_SIZE 4096
#define HASH_MASK (HASH_SIZE - 1)

typedef struct string_t
{
  char* string;
  struct string_t* next;
} string_t;

typedef struct stringtab_t
{
  string_t* string[HASH_SIZE];
} stringtab_t;

static stringtab_t table;

const char* stringtab( const char* string )
{
  if( string == NULL ) { return NULL; }

  uint64_t hash = strhash( string ) & HASH_MASK;
  string_t* cur = table.string[hash];

  while( cur != NULL )
  {
    if( !strcmp( cur->string, string ) )
    {
      return cur->string;
    }

    cur = cur->next;
  }

  cur = malloc( sizeof(string_t) );
  cur->string = strdup( string );
  cur->next = table.string[hash];
  table.string[hash] = cur;

  return cur->string;
}

void stringtab_done()
{
  string_t* cur;
  string_t* next;

  for( int i = 0; i < HASH_SIZE; i++ )
  {
    cur = table.string[i];

    while( cur != NULL )
    {
      next = cur->next;
      free( cur->string );
      free( cur );
      cur = next;
    }
  }

  memset( table.string, 0, HASH_SIZE * sizeof(string_t*) );
}
