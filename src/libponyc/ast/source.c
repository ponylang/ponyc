#include "source.h"
#include "error.h"
#include "../ds/stringtab.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "../platform/platform.h"


source_t* source_open(const char* file)
{
  intptr_t fd = pony_open(file, O_RDONLY);

  if(fd == -1)
  {
    errorf(file, "can't open file");
    return NULL;
  }

  struct stat sb;

  if(fstat(fd, &sb) < 0)
  {
    errorf(file, "can't determine length of file");
    pony_close(fd);
    return NULL;
  }

  char* m = map_file(sb.st_size, PONY_PROT_READ, fd);
  pony_close(fd);

  if(m == PONY_MAP_FAILED)
  {
    errorf(file, "can't read file");
    return NULL;
  }

  source_t* source = (source_t*)malloc(sizeof(source_t));
  source->file = stringtab(file);
  source->m = m;
  source->len = sb.st_size;

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

  if(source->file != NULL)
    unmap_file(source->m, source->len);
  else
    free(source->m);

  free(source);
}
