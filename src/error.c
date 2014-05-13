#include "error.h"
#include "stringtab.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define LINE_LEN 1024

static error_t* errors;

error_t* get_errors()
{
  return errors;
}

void free_errors()
{
  error_t* e = errors;
  errors = NULL;

  while( e != NULL )
  {
    error_t* next = e->next;
    free( e );
    e = next;
  }
}

void print_errors()
{
  error_t* e = errors;

  while( e != NULL )
  {
    printf( "%s:", e->file );

    if( e->line != 0 )
    {
      printf( "%ld:%ld:", e->line, e->pos );
    } else {
      printf( " " );
    }

    printf( "%s\n", e->msg );

    if( e->source != NULL )
    {
      printf( "%s\n", e->source );

      for( size_t i = 1; i < e->pos; i++ )
      {
        printf( " " );
      }

      printf( "^\n" );
    }

    e = e->next;
  }
}

void errorv( source_t* source, size_t line, size_t pos, const char* fmt,
  va_list ap )
{
  assert( source != NULL );

  char buf[LINE_LEN];
  vsnprintf( buf, LINE_LEN, fmt, ap );

  error_t* e = malloc(sizeof(error_t));
  e->file = stringtab( source->file );
  e->line = line;
  e->pos = pos;
  e->msg = stringtab( buf );
  e->source = NULL;
  e->next = errors;
  errors = e;

  if( line != 0 )
  {
    size_t tline = 1;
    size_t tpos = 0;

    while( (tline < e->line) && (tpos < source->len) )
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

    snprintf( buf, LINE_LEN, "%.*s", (int)(end - start), &source->m[start] );
    e->source = stringtab( buf );
  }
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
  char buf[LINE_LEN];

  va_list ap;
  va_start( ap, fmt );
  vsnprintf( buf, LINE_LEN, fmt, ap );
  va_end( ap );

  error_t* e = malloc(sizeof(error_t));
  e->file = stringtab( file );
  e->line = 0;
  e->pos = 0;
  e->msg = stringtab( buf );
  e->source = NULL;
  e->next = errors;
  errors = e;
}
