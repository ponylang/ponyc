#include "options.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

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

static bool is_positional(parse_state_t* s)
{
  return (s->argv[s->idx][0] != '-' || s->argv[s->idx][1] == '\0');
}

static arg_t* find_match(parse_state_t* s)
{
  arg_t* args = s->args;
  arg_t* match = NULL;
  size_t len = (size_t)(s->opt_end - s->opt_start);

  bool exact = false;
  bool ambig = false;

  int mode = MATCH_INIT;
  size_t match_len;
  char* name;

  while(mode < MATCH_NONE)
  {
    s->match_type = ++mode;

    for(arg_t* p = args; !END_MARKER(p); ++p)
    {
      name = (mode == MATCH_LONG) ? p->long_opt : &p->short_opt;
      match_len = (mode == MATCH_LONG) ? len : 1;

      if (!strncmp(name, s->opt_start, (mode == MATCH_LONG) ? len : 1))
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

  s->opt_start = NULL;
  s->opt_end = NULL;
  s->match_type = 0;
  s->idx = 0;
}

int opt_next(parse_state_t* s)
{
  int remove = 0;

  if(s->opt_start == NULL || *s->opt_start == '\0')
  {
    //Parsing a new option, advance in argv.
    //If this is the first call to opt_next,
    //skip argv[0] (the program name).
    s->idx++;

    //skip all non-options
    while(true)
    {
      if(s->idx == *s->argc)
        return -1;

      if(is_positional(s))
      {
        s->idx++;
        continue;
      }

      break;
    }
  }

  //Found a named argument
  s->opt_start = (s->argv[s->idx] + 1 + (s->argv[s->idx][1] == '-'));
  for(s->opt_end = s->opt_start; *s->opt_end && *s->opt_end != '='; s->opt_end++);

  //Check for exact or abbreviated match. If the option is known, process it,
  //otherwise ignore it.
  arg_t* m = find_match(s);

  if(m == NULL)
  {
    s->idx++;
    return opt_next(s);
  }
  else if(m == (arg_t*)1)
  {
    printf("%s: '%s' option is ambiguous!\n", s->argv[0], s->argv[s->idx]);
    return -2;
  }

  bool short_arg = (s->match_type == MATCH_SHORT && (*(s->opt_start + 1) ||
    s->idx < *s->argc));

  bool long_arg = (s->match_type == MATCH_LONG && ((*s->opt_end == '=')
    || s->idx < *s->argc));

  bool has_arg = (short_arg | long_arg);

  if ((m->flag == ARGUMENT_REQUIRED) && !has_arg)
  {
    printf("%s: '%s' option requires an argument!\n", s->argv[0],
      s->argv[s->idx]);

    return -2;
  }

  switch(s->match_type)
  {
    case MATCH_LONG:
      if(m->flag == ARGUMENT_REQUIRED)
      {
        if(*s->opt_end == '=')
        {
          s->arg_val = s->opt_end + 1;
          s->opt_start += strlen(s->opt_start);
        }
        else
        {
          s->arg_val = s->argv[s->idx + 1];
          s->opt_start += strlen(s->opt_start);
          remove++;
        }
      }
      remove++;
      break;
    case MATCH_SHORT:
      //strip out the short option, as short options may be
      //grouped
      memmove(s->opt_start, s->opt_start + 1, strlen(s->opt_start));

      if(*s->opt_start)
        s->opt_start = s->argv[s->idx];
      else
        remove++;

      if(m->flag == ARGUMENT_REQUIRED)
      {
        if(*s->opt_end)
        {
          s->arg_val = s->opt_end;
          s->opt_start += strlen(s->opt_start);
        }
        else if (*(s->opt_start) != '-')
        {
          s->arg_val = s->argv[s->idx + 1];
        }
        else
        {
          s->arg_val = s->opt_start + 1;
          s->opt_start += strlen(s->opt_start);
        }

        remove++;
      }
      break;
  }

  if (remove > 0)
  {
    *s->argc -= remove;

    memmove(&s->argv[s->idx], &s->argv[s->idx + remove],
      (*s->argc - s->idx) * sizeof(char*));

    s->idx--;
  }

  return m->id;
}
