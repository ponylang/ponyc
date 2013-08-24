#ifndef SOURCE_H
#define SOURCE_H

#include <stddef.h>

typedef struct source_t
{
  const char* file;
  char* m;
  size_t len;
} source_t;

source_t* source_open( const char* file );
void source_close( source_t* source );

#endif
