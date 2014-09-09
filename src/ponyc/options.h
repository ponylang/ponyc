#ifndef PONYC_OPTIONS_H
#define PONYC_OPTIONS_H

#include <stdint.h>

#define ARGUMENTS_FINISH \
  {NULL, 0, UINT32_MAX, UINT32_MAX}

enum
{
  ARGUMENT_REQUIRED = 0x01,
  ARGUMENT_NONE     = 0x02
};

typedef struct arg_t
{
  char* long_opt;
  char short_opt;
  uint32_t flag;
  uint32_t id;
} arg_t;

typedef struct parse_state_t
{
  int* argc;
  char** argv;
  arg_t* args;
  char* arg_val;
} parse_state_t;

void opt_init(arg_t* args, parse_state_t* s, int* argc, char** argv);

int opt_next(parse_state_t* s);

#endif
