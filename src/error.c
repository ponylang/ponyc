#include "error.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void errorv( source_t* source, size_t line, size_t pos, const char* fmt,
  va_list ap )
{
  if( source != NULL )
  {
    printf( "%s", source->file );

    if( line != 0 )
    {
      printf( " [%ld:%ld]", line, pos );
    }

    printf( ": " );
  }

  vprintf( fmt, ap );
  printf( "\n" );

  if( (source == NULL) || (line == 0) )
  {
    return;
  }

  size_t tline = 1;
  size_t tpos = 0;

  while( (tline < line) && (tpos < source->len) )
  {
    if( source->m[tpos] == '\n' )
    {
      tline++;
    }

    tpos++;
  }

  size_t start = tpos;

  while( (source->m[tpos] != '\n') && (tpos < source->len) )
  {
    tpos++;
  }

  size_t end = tpos;

  printf( "%.*s\n", (int)(end - start), &source->m[start] );

  for( size_t i = 1; i < pos; i++ )
  {
    printf( " " );
  }

  printf( "^\n" );
}

void error( source_t* source, size_t line, size_t pos, const char* fmt, ... )
{
  va_list ap;
  va_start( ap, fmt );
  errorv( source, line, pos, fmt, ap );
  va_end( ap );
}

void errorf( const char* file, const char* fmt, ... )
{
  va_list ap;
  va_start( ap, fmt );
  errorv( NULL, 0, 0, fmt, ap );
  va_end( ap );
}
