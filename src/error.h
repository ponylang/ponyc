#ifndef ERROR_H
#define ERROR_H

#include "source.h"
#include <stddef.h>
#include <stdarg.h>

void errorv( source_t* source, size_t line, size_t pos, const char* fmt,
  va_list ap );

void error( source_t* source, size_t line, size_t pos, const char* fmt, ... )
  __attribute__ ((format (printf, 4, 5)));

void errorf( const char* file, const char* fmt, ... )
  __attribute__ ((format (printf, 2, 3)));

#endif
