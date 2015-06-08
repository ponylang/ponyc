#include "options.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define MATCH_LONG  1
#define MATCH_SHORT 2
#define MATCH_NONE  3
#define PARSE_ARG (OPT_ARG_REQUIRED | OPT_ARG_OPTIONAL)

static bool end_reached(const opt_arg_t* arg)
{
  return (arg->long_opt == 0) && (arg->short_opt == 0) &&
    (arg->id == UINT32_MAX) && (arg->flag == UINT32_MAX);
}

static bool has_argument(opt_state_t* s)
{
  bool short_arg = ((s->match_type == MATCH_SHORT) &&
    (*(s->opt_start + 1) || s->idx < (*s->argc)-1));

  bool long_arg = ((s->match_type == MATCH_LONG) &&
    ((*s->opt_end == '=') || s->idx < (*s->argc)-1));

  return (short_arg | long_arg);
}

static void parse_option_name(opt_state_t* s)
{
  s->opt_start = (s->argv[s->idx] + 1 + (s->argv[s->idx][1] == '-'));

  for(s->opt_end = s->opt_start; *s->opt_end && *s->opt_end != '=';
    s->opt_end++);
}

static const opt_arg_t* find_match(opt_state_t* s)
{
  bool ambig = false;
  size_t match_length;

  const char* match_name;
  const opt_arg_t* match = NULL;

  parse_option_name(s);

  for(s->match_type = MATCH_LONG; s->match_type <= MATCH_SHORT; ++s->match_type)
  {
    for(const opt_arg_t* p = s->args; !end_reached(p); ++p)
    {
      if(s->match_type == MATCH_LONG)
      {
        match_name = p->long_opt;
        match_length = (size_t)(s->opt_end - s->opt_start);
      }
      else
      {
        match_name = &p->short_opt;
        match_length = 1;
      }

      if(!strncmp(match_name, s->opt_start, match_length))
      {
        if(match_length == strlen(match_name))
        {
          // Exact match found. It is necessary to check for
          // the length of p->long_opt since there might be
          // options that are prefixes of another (strncmp),
          // and short options might be grouped.
          match = p;
          break;
        }
        else if((match != NULL) && (match->id != p->id))
        {
          ambig = true;
        }
      }
    }

    if(ambig && s->match_type == MATCH_SHORT)
      return (opt_arg_t*)1;
    else if(match != NULL)
      return match;
  }

  s->match_type = MATCH_NONE;
  return NULL;
}

static bool is_positional(const opt_state_t* s)
{
  return (s->argv[s->idx][0] != '-' || s->argv[s->idx][1] == '\0');
}

static bool skip_non_options(opt_state_t* s)
{
  while(true)
  {
    if(s->idx == *s->argc)
      return false;

    if(!is_positional(s))
      return true;

    s->idx++;
  }
}

static void strip_accepted_opts(opt_state_t* s)
{
  if(s->remove > 0)
  {
    *s->argc -= s->remove;

    memmove(&s->argv[s->idx], &s->argv[s->idx + s->remove],
      (*s->argc - s->idx) * sizeof(char*));

    s->idx--;

    s->remove = 0;
  }
}

static void parse_long_opt_arg(opt_state_t* s)
{
  if(*s->opt_end == '=')
  {
    s->arg_val = s->opt_end + 1;
    s->opt_start += strlen(s->opt_start);
  }
  else if(s->argv[s->idx + 1][0] != '-')
  {
    s->arg_val = s->argv[s->idx + 1];
    s->opt_start += strlen(s->opt_start);

    // Only remove if there actually was an argument
    if(s->argv[s->idx + 1][0])
      s->remove++;
  }
}

static void parse_short_opt_arg(opt_state_t* s)
{
  if(*s->opt_end)
  {
    s->arg_val = s->opt_end;
    s->opt_start += strlen(s->opt_start);
  }
  else if(*(s->opt_start) != '-')
  {
    s->arg_val = s->argv[s->idx + 1];
  }
  else
  {
    s->arg_val = s->opt_start + 1;
    s->opt_start += strlen(s->opt_start);
  }

  s->remove++;
}

static void parse_short_opt(opt_state_t* s)
{
  // Strip out the short option, as short options may be
  // grouped
  memmove(s->opt_start, s->opt_start + 1, strlen(s->opt_start));

  if(*s->opt_start)
    s->opt_start = s->argv[s->idx];
  else
    s->remove++;
}

static int missing_argument(opt_state_t* s)
{
   printf("%s: '%s' option requires an argument!\n", s->argv[0],
      s->argv[s->idx]);

    return -2;
}

void opt_init(const opt_arg_t* args, opt_state_t* s, int* argc, char** argv)
{
  s->argc = argc;
  s->argv = argv;
  s->args = args;
  s->arg_val = NULL;

  s->opt_start = NULL;
  s->opt_end = NULL;
  s->match_type = 0;
  s->idx = 0;
  s->remove = 0;
}

int opt_next(opt_state_t* s)
{
  s->arg_val = NULL;
  
  if(s->opt_start == NULL || *s->opt_start == '\0')
  {
    // Parsing a new option
    s->idx++;

    if(!skip_non_options(s))
      return -1;
  }

  // Check for known exact match. If the option is known, process it,
  // otherwise ignore it.
  const opt_arg_t* m = find_match(s);

  if(m == NULL)
  {
    s->opt_start += strlen(s->opt_start);
    return opt_next(s);
  }
  else if(m == (opt_arg_t*)1)
  {
    printf("%s: '%s' option is ambiguous!\n", s->argv[0], s->argv[s->idx]);
    return -2;
  }

  if ((m->flag == OPT_ARG_REQUIRED) && !has_argument(s))
    return missing_argument(s);
  
  if(s->match_type == MATCH_LONG)
  {
    s->remove++;

    if((m->flag & PARSE_ARG) && has_argument(s))
    {
      parse_long_opt_arg(s);

      if(s->arg_val == NULL && (m->flag & OPT_ARG_REQUIRED))
        return missing_argument(s);
    }

    s->opt_start = NULL;
  }
  else if(s->match_type == MATCH_SHORT)
  {
    parse_short_opt(s);

    if((m->flag & PARSE_ARG) && has_argument(s))
    {
      parse_short_opt_arg(s);
      
      if(s->arg_val == NULL && (m->flag & OPT_ARG_REQUIRED))
        return missing_argument(s);
    }
  }

  strip_accepted_opts(s);

  return m->id;
}
