#ifndef ERROR_H
#define ERROR_H

#include <stddef.h>

typedef struct error_t
{
  size_t line;
  size_t pos;
  char *msg;
  struct error_t* next;
} error_t;

typedef struct errorlist_t
{
  error_t* head;
  error_t* tail;
} errorlist_t;

error_t* error_new(
  errorlist_t* list,
  size_t line,
  size_t pos,
  const char* fmt, ... ) __attribute__ ((format (printf, 4, 5)));

void error_free( error_t* error );

errorlist_t* errorlist_new();
void errorlist_free( errorlist_t* list );

#endif
