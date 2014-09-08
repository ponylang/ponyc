#include "options.h"

#include <string.h>
#include <stdio.h>

static char* opt_start = NULL;
static char* opt_end = NULL;
static int match_type;
static int index;

#define END_MARKER(X) \
  ((X->long_opt == NULL) && \
  (X->short_opt == 0) && \
  (X->id == UINT32_MAX) && \
  (X->flag == UINT32_MAX))

enum
{
  MATCH_INIT = UINT32_MAX,
  MATCH_LONG,
  MATCH_SHORT,
  MATCH_NONE
};

static bool is_positional(char** argv)
{
  return (argv[index][0] != '-' || argv[index][1] == '\0');
}

static arg_t* find_match(arg_t* args, char* opt_start, char* opt_end)
{
  arg_t* match = NULL;
  size_t len = (size_t)(opt_end - opt_start);

  bool exact = false;
  bool ambig = false;

  int mode = MATCH_INIT;
  char* name;

  while(mode < MATCH_NONE)
  {
    match_type = ++mode;

    for(arg_t* p = args; !END_MARKER(p); ++p)
    {
      name = (mode == MATCH_LONG) ? p->long_opt : &p->short_opt;

      if(!strncmp(name, opt_start, (mode == MATCH_LONG) ? len : 1))
      {
        if(len == strlen(name))
        {
          //Exact match found. It is necessary to check for
          //the length of p->long_opt since there might be
          //options that are prefixes of another (strncmp).
          exact = true;
          match = p;
          break;
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

    if(ambig && !exact)
      return (arg_t*)1;
    else if(match != NULL)
      return match;
  }

  match_type = MATCH_NONE;
  return NULL;
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

  if(index == *s->argc)
    return -1;

  if(opt_start == NULL || *opt_start == '\0')
  {
    //Parsing a new option, advance in argv.
    //If this is the first call to opt_next,
    //skip argv[0] (the program name).
    index++;

    //skip all non-options
    while(true)
    {
      if(is_positional(s->argv))
      {
        index++;
        continue;
      }

      break;
    }
  }

  //Found a named argument
  opt_start = (s->argv[index] + 1 + (s->argv[index][1] == '-'));
  for(opt_end = opt_start; *opt_end && *opt_end != '='; opt_end++);

  //Check for exact or abbreviated match. If the option is known, process it,
  //otherwise ignore it.
  arg_t* m = find_match(s->args, opt_start, opt_end);

  if(m == NULL)
  {
    index++;
    return opt_next(s);
  }
  else if(m == (arg_t*)1)
  {
    //ambiguous match
    printf("%s: '%s' option is ambiguous!\n", s->argv[0], s->argv[index]);
    return -2;
  }

  remove++;

  if(*opt_end == '=')
  {
    if(m->flag & ARGUMENT_NONE)
    {
      printf("%s: '%s' option does not take an argument!\n", s->argv[0],
        s->argv[index]);

      return -2;
    }
    else
    {
      //the argument is the rest of the current string
      //to the next '\0'.
      s->arg_val = opt_end + 1;
    }
  }
  else if(m->flag & ARGUMENT_REQUIRED)
  {
    if(index < *s->argc)
    {
      s->arg_val = s->argv[++index];
      remove++;
    }
    else
    {
      printf("%s: '%s' option requires an argument!\n", s->argv[0],
        s->argv[index]);

      return -2;
    }
  }

  //If execution reaches this point, a known and valid named argument
  //was found. Return it to the user, and remove it from the argv array.
  switch (match_type)
  {
    case MATCH_LONG:
      opt_start += strlen(opt_start);
      break;
    case MATCH_SHORT:
      //strip out the short option, as short options
      //may be grouped.
      memmove(opt_start, opt_start+1,
        strlen(opt_start));

      opt_start = s->argv[index];

      break;
   }
  
  *s->argc -= remove;
  index -= remove;

  memmove(&s->argv[index], &s->argv[index + remove], 
    (*s->argc - index) * sizeof(char*));

  return m->id;
}
