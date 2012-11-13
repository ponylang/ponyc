#include "error.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

error_t* error_new( errorlist_t* list, size_t line, size_t pos, const char* fmt, ... )
{
  error_t* error = malloc( sizeof(error_t) );
  error->line = line;
  error->pos = pos;
  error->next = NULL;

  va_list ap;
  char buffer[256];

  va_start( ap, fmt );
  int n = vsnprintf( buffer, 256, fmt, ap );
  va_end( ap );

  if( n < 0 )
  {
    error->msg = strdup( "PANIC" );
  } else if( n < 256 ) {
    error->msg = strdup( buffer );
  } else {
    error->msg = malloc( n + 1 );
    va_start( ap, fmt );
    vsnprintf( error->msg, n + 1, fmt, ap );
    va_end( ap );
  }

  if( list != NULL )
  {
    if( list->tail != NULL )
    {
      list->tail->next = error;
      list->tail = error;
    } else {
      list->head = error;
      list->tail = error;
    }
  }

  return error;
}

void error_free( error_t* error )
{
  if( error == NULL ) { return; }
  if( error->msg != NULL ) { free( error->msg ); }
  free( error );
}

errorlist_t* errorlist_new()
{
  return calloc( 1, sizeof(errorlist_t) );
}

void errorlist_free( errorlist_t* list )
{
  if( list == NULL ) { return; }
  list->tail = NULL;

  error_t* e;

  while( (e = list->head) != NULL )
  {
    list->head = e->next;
    error_free( e );
  }

  free( list );
}
