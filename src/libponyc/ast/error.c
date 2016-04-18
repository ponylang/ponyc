#include "error.h"
#include "stringtab.h"
#include "../../libponyrt/mem/pool.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define LINE_LEN 1024


typedef struct errors_t
{
  errormsg_t* head;
  errormsg_t* tail;
  size_t count;
  bool immediate_report;
  FILE* output_stream;
} errors_t;


errormsg_t* error_alloc()
{
  errormsg_t* e = POOL_ALLOC(errormsg_t);
  memset(e, 0, sizeof(errormsg_t));
  return e;
}

static void error_free(errormsg_t* e)
{
  while(e != NULL)
  {
    errormsg_t* next = e->frame;
    POOL_FREE(errormsg_t, e);
    e = next;
  }
}

errors_t* errors_alloc()
{
  errors_t* errors = POOL_ALLOC(errors_t);
  memset(errors, 0, sizeof(errors_t));
  errors->output_stream = stdout;
  return errors;
}

void errors_free(errors_t* errors)
{
  errormsg_t* e = errors->head;

  while(e != NULL)
  {
    errormsg_t* next = e->next;
    error_free(e);
    e = next;
  }

  POOL_FREE(errors_t, errors);
}

errormsg_t* errors_get_first(errors_t* errors)
{
  return errors->head;
}

size_t errors_get_count(errors_t* errors)
{
  return errors->count;
}

void errors_set_immediate(errors_t* errors, bool immediate)
{
  errors->immediate_report = immediate;
}

void errors_set_output_stream(errors_t* errors, FILE* fp)
{
  errors->output_stream = fp;
}

static void error_print_msg(errormsg_t* e, FILE* fp, const char* indent)
{
  if(e->file != NULL)
  {
    fprintf(fp, "%s%s:", indent, e->file);

    if(e->line != 0)
    {
      fprintf(fp, __zu ":" __zu ": ", e->line, e->pos);
    }
    else {
      fprintf(fp, " ");
    }
  }

  fprintf(fp, "%s\n", e->msg);

  if(e->source != NULL)
  {
    fprintf(fp, "%s%s\n", indent, e->source);
    fprintf(fp, "%s", indent);

    for(size_t i = 0; i < (e->pos - 1); i++)
    {
      if(e->source[i] == '\t')
        fprintf(fp, "\t");
      else
        fprintf(fp, " ");
    }

    fprintf(fp, "^\n");
  }
}

static void error_print(errormsg_t* e, FILE* fp)
{
  fprintf(fp, "Error:\n");
  error_print_msg(e, fp, "");

  if(e->frame != NULL)
  {
    fprintf(fp, "    Info:\n");

    for(errormsg_t* ef = e->frame; ef != NULL; ef = ef->frame)
      error_print_msg(ef, fp, "    ");
  }
}

static void errors_push(errors_t* errors, errormsg_t* e)
{
  if(errors->immediate_report)
    error_print(e, errors->output_stream);

  if(errors->head == NULL)
  {
    errors->head = e;
    errors->tail = e;
  } else {
    errors->tail->next = e;
    errors->tail = e;
  }

  e->next = NULL;
  errors->count++;
}

static void append_to_frame(errorframe_t* frame, errormsg_t* e)
{
  assert(frame != NULL);

  if(*frame == NULL)
  {
    *frame = e;
  }
  else
  {
    errormsg_t* p = *frame;
    while(p->frame != NULL)
      p = p->frame;

    p->frame = e;
  }
}

void errors_print(errors_t *errors)
{
  if(errors->immediate_report)
    return;

  errormsg_t* e = errors->head;

  while(e != NULL)
  {
    error_print(e, errors->output_stream);
    e = e->next;
  }
}

static errormsg_t* make_errorv(source_t* source, size_t line, size_t pos,
  const char* fmt, va_list ap)
{
  char buf[LINE_LEN];
  vsnprintf(buf, LINE_LEN, fmt, ap);

  errormsg_t* e = error_alloc();

  if(source != NULL)
    e->file = source->file;

  e->line = line;
  e->pos = pos;
  e->msg = stringtab(buf);

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

    if(len >= sizeof(buf))
      len = sizeof(buf) - 1;

    memcpy(buf, &source->m[start], len);
    buf[len] = '\0';
    e->source = stringtab(buf);
  }

  return e;
}

void errorv(errors_t* errors, source_t* source, size_t line, size_t pos,
  const char* fmt, va_list ap)
{
  errors_push(errors, make_errorv(source, line, pos, fmt, ap));
}

void errorv_continue(errors_t* errors, source_t* source, size_t line,
  size_t pos, const char* fmt, va_list ap)
{
  if(errors->tail == NULL)
    return errorv(errors, source, line, pos, fmt, ap);

  errormsg_t* p = errors->tail;
  while(p->frame != NULL)
    p = p->frame;

  p->frame = make_errorv(source, line, pos, fmt, ap);
}

void error(errors_t* errors, source_t* source, size_t line, size_t pos,
  const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  errorv(errors, source, line, pos, fmt, ap);
  va_end(ap);
}

void error_continue(errors_t* errors, source_t* source, size_t line, size_t pos,
  const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  errorv_continue(errors, source, line, pos, fmt, ap);
  va_end(ap);
}

void errorframev(errorframe_t* frame, source_t* source, size_t line,
  size_t pos, const char* fmt, va_list ap)
{
  assert(frame != NULL);
  append_to_frame(frame, make_errorv(source, line, pos, fmt, ap));
}

void errorframe(errorframe_t* frame, source_t* source, size_t line, size_t pos,
  const char* fmt, ...)
{
  assert(frame != NULL);

  va_list ap;
  va_start(ap, fmt);
  errorframev(frame, source, line, pos, fmt, ap);
  va_end(ap);
}

static errormsg_t* make_errorfv(const char* file, const char* fmt, va_list ap)
{
  char buf[LINE_LEN];
  vsnprintf(buf, LINE_LEN, fmt, ap);

  errormsg_t* e = error_alloc();

  e->file = stringtab(file);
  e->msg = stringtab(buf);
  return e;
}

void errorfv(errors_t* errors, const char* file, const char* fmt, va_list ap)
{
  errors_push(errors, make_errorfv(file, fmt, ap));
}

void errorf(errors_t* errors, const char* file, const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  errorfv(errors, file, fmt, ap);
  va_end(ap);
}

void errorframe_append(errorframe_t* first, errorframe_t* second)
{
  assert(first != NULL);
  assert(second != NULL);

  append_to_frame(first, *second);
  *second = NULL;
}

bool errorframe_has_errors(errorframe_t* frame)
{
  assert(frame != NULL);
  return frame != NULL;
}

void errorframe_report(errorframe_t* frame, errors_t* errors)
{
  assert(frame != NULL);

  if(*frame != NULL)
    errors_push(errors, *frame);

  *frame = NULL;
}

void errorframe_discard(errorframe_t* frame)
{
  assert(frame != NULL);

  error_free(*frame);
  *frame = NULL;
}
