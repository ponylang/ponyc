#ifndef ERROR_H
#define ERROR_H

#include "source.h"
#include <stddef.h>
#include <stdarg.h>

typedef struct errormsg_t
{
  const char* file;
  size_t line;
  size_t pos;
  const char* msg;
  const char* source;
  struct errormsg_t* next;
} errormsg_t;

errormsg_t* get_errors();

size_t get_error_count();

void free_errors();

void print_errors();

void errorv(source_t* source, size_t line, size_t pos, const char* fmt,
  va_list ap);

void error(source_t* source, size_t line, size_t pos, const char* fmt, ...)
  __attribute__ ((format (printf, 4, 5)));

void errorfv(const char* file, const char* fmt, va_list ap);

void errorf(const char* file, const char* fmt, ...)
  __attribute__ ((format (printf, 2, 3)));

#endif
