#include "error.h"
#include "../ds/stringtab.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define LINE_LEN 1024

static errormsg_t* head = NULL;
static errormsg_t* tail = NULL;
static size_t count = 0;

static void add_error(errormsg_t* e)
{
  if(head == NULL)
  {
    head = e;
    tail = e;
  } else {
    tail->next = e;
    tail = e;
  }

  e->next = NULL;
  count++;
}

errormsg_t* get_errors()
{
  return head;
}

size_t get_error_count()
{
  return count;
}

void free_errors()
{
  errormsg_t* e = head;
  head = NULL;
  tail = NULL;

  while(e != NULL)
  {
    errormsg_t* next = e->next;
    free(e);
    e = next;
  }

  count = 0;
}

void print_errors()
{
  errormsg_t* e = head;

  while(e != NULL)
  {
    if(e->file != NULL)
    {
      printf("%s:", e->file);

      if(e->line != 0)
      {
        printf("%ld:%ld: ", e->line, e->pos);
      } else {
        printf(" ");
      }
    }

    printf("%s\n", e->msg);

    if(e->source != NULL)
    {
      printf("%s\n", e->source);

      for(size_t i = 1; i < e->pos; i++)
        printf(" ");

      printf("^\n");
    }

    e = e->next;
  }
}

void errorv(source_t* source, size_t line, size_t pos, const char* fmt,
  va_list ap)
{
  char buf[LINE_LEN];
  vsnprintf(buf, LINE_LEN, fmt, ap);

  errormsg_t* e = (errormsg_t*)calloc(1, sizeof(errormsg_t));

  if(source != NULL)
    e->file = source->file;

  e->line = line;
  e->pos = pos;
  e->msg = stringtab(buf);
  add_error(e);

  if((source != NULL) && (line != 0))
  {
    size_t tline = 1;
    size_t tpos = 0;

    while((tline < e->line) && (tpos < source->len))
    {
      if(source->m[tpos] == '\n')
        tline++;

      tpos++;
    }

    size_t start = tpos;

    while((source->m[tpos] != '\n') && (tpos < source->len))
      tpos++;

    size_t len = tpos - start;

    memcpy(buf, &source->m[start], len);
    buf[len] = '\0';
    e->source = stringtab(buf);
  }
}

void error(source_t* source, size_t line, size_t pos, const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  errorv(source, line, pos, fmt, ap);
  va_end(ap);
}

void errorfv(const char* file, const char* fmt, va_list ap)
{
  char buf[LINE_LEN];
  vsnprintf(buf, LINE_LEN, fmt, ap);

  errormsg_t* e = (errormsg_t*)calloc(1, sizeof(errormsg_t));
  e->file = stringtab(file);
  e->msg = stringtab(buf);
  add_error(e);
}

void errorf(const char* file, const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  errorfv(file, fmt, ap);
  va_end(ap);
}
