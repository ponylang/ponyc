#ifndef ERROR_H
#define ERROR_H

#include "source.h"
#include <stddef.h>
#include <stdarg.h>

typedef struct error_t
{
  const char* file;
  size_t line;
  size_t pos;
  const char* msg;
  const char* source;
  struct error_t* next;
} error_t;

error_t* get_errors();

void free_errors();

void print_errors();

void errorv(source_t* source, size_t line, size_t pos, const char* fmt,
  va_list ap);

void error(source_t* source, size_t line, size_t pos, const char* fmt, ...)
  __attribute__ ((format (printf, 4, 5)));

void errorf(const char* file, const char* fmt, ...)
  __attribute__ ((format (printf, 2, 3)));

#endif
