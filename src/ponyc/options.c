#include "options.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

static char* opt_start = NULL;
static char* opt_end = NULL;
static int match_type;
static int idx;

#define END_MARKER(X) \
  ((X->long_opt == NULL) && \
  (X->short_opt == 0) && \
  (X->id == UINT32_MAX) && \
  (X->flag == UINT32_MAX))

enum
{
  MATCH_INIT  = UINT32_MAX,
  MATCH_LONG  = 0,
  MATCH_SHORT = 1,
  MATCH_NONE  = 2
};

static bool is_positional(char** argv)
{
  return (argv[idx][0] != '-' || argv[idx][1] == '\0');
}

static arg_t* find_match(arg_t* args, char* opt_start, char* opt_end)
{
  arg_t* match = NULL;
  size_t len = (size_t)(opt_end - opt_start);

  bool exact = false;
  bool ambig = false;

  int mode = MATCH_INIT;
  size_t match_len;
  char* name;

  while(mode < MATCH_NONE)
  {
    match_type = ++mode;

    for(arg_t* p = args; !END_MARKER(p); ++p)
    {
      name = (mode == MATCH_LONG) ? p->long_opt : &p->short_opt;
      match_len = (mode == MATCH_LONG) ? len : 1;

      if (!strncmp(name, opt_start, (mode == MATCH_LONG) ? len : 1))
      {
        if(match_len == strlen(name))
        {
          //Exact match found. It is necessary to check for
          //the length of p->long_opt since there might be
          //options that are prefixes of another (strncmp).
          exact = true;
          return p;
        }
        else if(match == NULL)
        {
          match = p;
        }
        else if(match->id != p->id)
        {
          ambig = true;
        }
      }
    }

    if (ambig && !exact && mode == MATCH_SHORT)
      return (arg_t*)1;
    else if (match != NULL && exact)
      return match;
  }

  return match;
}

void opt_init(arg_t* args, parse_state_t* s, int* argc, char** argv)
{
  s->argc = argc;
  s->argv = argv;
  s->args = args;
  s->arg_val = NULL;
}

int opt_next(parse_state_t* s)
{
  int remove = 0;

  if(idx == *s->argc)
    return -1;

  if(opt_start == NULL || *opt_start == '\0')
  {
    //Parsing a new option, advance in argv.
    //If this is the first call to opt_next,
    //skip argv[0] (the program name).
    idx++;

    //skip all non-options
    while(true)
    {
      if(is_positional(s->argv))
      {
        idx++;
        continue;
      }

      break;
    }
  }

  //Found a named argument
  opt_start = (s->argv[idx] + 1 + (s->argv[idx][1] == '-'));
  for(opt_end = opt_start; *opt_end && *opt_end != '='; opt_end++);

  //Check for exact or abbreviated match. If the option is known, process it,
  //otherwise ignore it.
  arg_t* m = find_match(s->args, opt_start, opt_end);

  if(m == NULL)
  {
    idx++;
    return opt_next(s);
  }
  else if(m == (arg_t*)1)
  {
    printf("%s: '%s' option is ambiguous!\n", s->argv[0], s->argv[idx]);
    return -2;
  }

  bool short_arg = (match_type == MATCH_SHORT && *(opt_start + 1));
  bool long_arg = (match_type == MATCH_LONG && ((*opt_end == '=') || idx < *s->argc));
  bool has_arg = (short_arg | long_arg);

  if ((m->flag == ARGUMENT_REQUIRED) && !has_arg)
  {
    printf("%s: '%s' option requires an argument!\n", s->argv[0],
      s->argv[idx]);

    return -2;
  }

  switch(match_type)
  {
    case MATCH_LONG:
      if(m->flag == ARGUMENT_REQUIRED)
      {
        if(*opt_end == '=')
        {
          s->arg_val = opt_end + 1;
          opt_start += strlen(opt_start);
        }
        else
        {
          s->arg_val = s->argv[++idx];
          opt_start += strlen(opt_start);
          remove++;
        }
      }
      remove++;
      break;
    case MATCH_SHORT:
      //strip out the short option, as short options may be
      //grouped
      memmove(opt_start, opt_start + 1, strlen(opt_start));
      
      if(*opt_start)
        opt_start = s->argv[idx];
      else
        remove++;

      if(m->flag == ARGUMENT_REQUIRED)
      {
        if(*opt_end == '=')
        {
          s->arg_val = opt_end + 1;
          opt_start += strlen(opt_start);
        }
        else if (*(opt_start) != '-')
        {
          s->arg_val = s->argv[++idx];
        }
        else
        {
          s->arg_val = opt_start + 1;
        }

        remove++;
      }
      break;
  }

  if (remove > 0)
  {
    *s->argc -= remove;
    idx -= remove;

    memmove(&s->argv[idx+1], &s->argv[idx + 1 + remove],
      (*s->argc - idx) * sizeof(char*));
  }

  return m->id;
}
