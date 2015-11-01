#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdint.h>

#define OPT_ARG_REQUIRED 1 << 0
#define OPT_ARG_OPTIONAL 1 << 1
#define OPT_ARG_NONE     1 << 2
#define OPT_ARGS_FINISH {NULL, 0, UINT32_MAX, UINT32_MAX}

typedef struct opt_arg_t
{
  const char* long_opt;
  const char short_opt;
  uint32_t flag;
  uint32_t id;
} opt_arg_t;

typedef struct opt_state_t
{
  const opt_arg_t* args;

  int* argc;
  char** argv;
  char* arg_val;

  //do not touch :-)
  char* opt_start;
  char* opt_end;
  int match_type;
  int idx;
  int remove;
} opt_state_t;

void opt_init(const opt_arg_t* args, opt_state_t* s, int* argc, char** argv);

int opt_next(opt_state_t* s);

#endif
