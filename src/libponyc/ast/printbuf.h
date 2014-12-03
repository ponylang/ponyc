#ifndef PRINTBUF_H
#define PRINTBUF_H

#include <stddef.h>
#include <stdarg.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN

typedef struct printbuf_t
{
  char* m;
  size_t size;
  size_t offset;
} printbuf_t;

printbuf_t* printbuf_new();

void printbuf_free(printbuf_t* buf);

void printbuf(printbuf_t* buf, const char* fmt, ...);

PONY_EXTERN_C_END

#endif
