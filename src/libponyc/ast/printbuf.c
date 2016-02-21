#include "printbuf.h"
#include "../../libponyrt/mem/pool.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

printbuf_t* printbuf_new()
{
  printbuf_t* buf = POOL_ALLOC(printbuf_t);
  buf->m = (char*)malloc(32);
  buf->m[0] = '\0';
  buf->size = 32;
  buf->offset = 0;
  return buf;
}

void printbuf_free(printbuf_t* buf)
{
  free(buf->m);
  POOL_FREE(printbuf_t, buf);
}

void printbuf(printbuf_t* buf, const char* fmt, ...)
{
  size_t avail = buf->size - buf->offset;
  va_list ap;

  va_start(ap, fmt);
  int r = vsnprintf(buf->m + buf->offset, avail, fmt, ap);
  va_end(ap);

  if(r < 0)
  {
#ifdef PLATFORM_IS_WINDOWS
    va_start(ap, fmt);
    r = _vscprintf(fmt, ap);
    va_end(ap);

    if(r < 0)
      return;
#else
    return;
#endif
  }

  if((size_t)r >= avail)
  {
    buf->size = buf->size + r + 1;
    buf->m = (char*)realloc(buf->m, buf->size);
    avail = buf->size - buf->offset;

    va_start(ap, fmt);
    r = vsnprintf(buf->m + buf->offset, avail, fmt, ap);
    va_end(ap);

    assert((r > 0) && ((size_t)r < buf->size));
  }

  buf->offset += r;
}
