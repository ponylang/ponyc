#include "source.h"
#include "error.h"
#include "../ds/stringtab.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


source_t* source_open(const char* file)
{
  int fd = open(file, O_RDONLY);

  if(fd == -1)
  {
    errorf(file, "can't open file");
    return NULL;
  }

  struct stat sb;

  if(fstat(fd, &sb) < 0)
  {
    errorf(file, "can't determine length of file");
    close(fd);
    return NULL;
  }

  char* m = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);

  if(m == MAP_FAILED)
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
  source->m = malloc(source->len);

  memcpy(source->m, source_code, source->len);

  return source;
}


void source_close(source_t* source)
{
  if(source == NULL)
    return;

  if(source->file != NULL)
    munmap(source->m, source->len);
  else
    free(source->m);

  free(source);
}
