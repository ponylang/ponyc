#include "source.h"
#include "error.h"
#include "../ds/stringtab.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

source_t* source_open(const char* file)
{
  FILE* fp = fopen(file, "rb");

  if(fp == NULL)
  {
    errorf(file, "can't open file");
    return NULL;
  }

  fseek(fp, 0, SEEK_END);
  ssize_t size = ftell(fp);

  if(size < 0)
  {
    errorf(file, "can't determine length of file");
    fclose(fp);
    return NULL;
  }

  fseek(fp, 0, SEEK_SET);

  source_t* source = (source_t*)malloc(sizeof(source_t));
  source->file = stringtab(file);
  source->m = (char*)malloc(size);
  source->len = size;

  ssize_t read = fread(source->m, sizeof(char), size, fp);

  if(read < size)
  {
    errorf(file, "failed to read entire file");
    free(source->m);
    free(source);
    fclose(fp);
    return NULL;
  }

  fclose(fp);
  return source;
}


source_t* source_open_string(const char* source_code)
{
  source_t* source = (source_t*)malloc(sizeof(source_t));
  source->file = NULL;
  source->len = strlen(source_code);
  source->m = (char*)malloc(source->len);

  memcpy(source->m, source_code, source->len);

  return source;
}


void source_close(source_t* source)
{
  if(source == NULL)
    return;

  if(source->m != NULL)
    free(source->m);

  free(source);
}
