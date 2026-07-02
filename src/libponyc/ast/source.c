#include "source.h"
#include "error.h"
#include "stringtab.h"
#include "../../libponyrt/mem/pool.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

source_t* source_open(const char* file, const char** error_msgp,
  strtable_t* strtab)
{
  FILE* fp = fopen(file, "rb");

  if(fp == NULL)
  {
    *error_msgp = "can't open file";
    return NULL;
  }

  // fopen("dir", "rb") succeeds on some platforms; the size logic below then
  // reads a meaningless length from ftell. On common Linux filesystems that
  // value is SSIZE_MAX, which overflows `size + 1` into a huge allocation that
  // aborts the compiler. Reject a directory here with a clear error. Source
  // discovery already skips directories; this is the backstop for any other
  // caller.
  struct stat s;
  if((stat(file, &s) == 0) && S_ISDIR(s.st_mode))
  {
    *error_msgp = "is a directory";
    fclose(fp);
    return NULL;
  }

  fseek(fp, 0, SEEK_END);
  ssize_t size = ftell(fp);

  if(size < 0)
  {
    *error_msgp = "can't determine length of file";
    fclose(fp);
    return NULL;
  }

  fseek(fp, 0, SEEK_SET);

  source_t* source = POOL_ALLOC(source_t);
  source->file = stringtab(strtab, file);
  source->m = (char*)ponyint_pool_alloc_size(size + 1);
  source->len = size + 1;

  ssize_t read = fread(source->m, sizeof(char), size, fp);
  source->m[size] = '\0';

  if(read < size)
  {
    *error_msgp = "failed to read entire file";
    ponyint_pool_free_size(source->len, source->m);
    POOL_FREE(source_t, source);
    fclose(fp);
    return NULL;
  }

  fclose(fp);
  return source;
}


source_t* source_open_string(const char* source_code)
{
  source_t* source = POOL_ALLOC(source_t);
  source->file = NULL;
  source->len = strlen(source_code) + 1; // for null terminator
  source->m = (char*)ponyint_pool_alloc_size(source->len);

  memcpy(source->m, source_code, source->len);

  return source;
}


void source_close(source_t* source)
{
  if(source == NULL)
    return;

  if(source->m != NULL)
    ponyint_pool_free_size(source->len, source->m);

  POOL_FREE(source_t, source);
}
