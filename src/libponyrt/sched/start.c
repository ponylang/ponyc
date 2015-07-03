#include "scheduler.h"
#include "../mem/heap.h"
#include "../gc/cycle.h"
#include "../lang/socket.h"
#include "../options/options.h"
#include <string.h>
#include <stdlib.h>

typedef struct options_t
{
  // concurrent options
  uint32_t threads;
  uint32_t cd_min_deferred;
  uint32_t cd_max_deferred;
  uint32_t cd_conf_group;
  size_t gc_initial;
  double gc_factor;
  bool noyield;
} options_t;

// global data
static int volatile exit_code;

enum
{
  OPT_THREADS,
  OPT_CDMIN,
  OPT_CDMAX,
  OPT_CDCONF,
  OPT_GCINITIAL,
  OPT_GCFACTOR,
  OPT_NOYIELD
};

static opt_arg_t args[] =
{
  {"ponythreads", 0, OPT_ARG_REQUIRED, OPT_THREADS},
  {"ponycdmin", 0, OPT_ARG_REQUIRED, OPT_CDMIN},
  {"ponycdmax", 0, OPT_ARG_REQUIRED, OPT_CDMAX},
  {"ponycdconf", 0, OPT_ARG_REQUIRED, OPT_CDCONF},
  {"ponygcinitial", 0, OPT_ARG_REQUIRED, OPT_GCINITIAL},
  {"ponygcfactor", 0, OPT_ARG_REQUIRED, OPT_GCFACTOR},
  {"ponynoyield", 0, OPT_ARG_NONE, OPT_NOYIELD},

  OPT_ARGS_FINISH
};

static int parse_opts(int argc, char** argv, options_t* opt)
{
  opt_state_t s;
  int id;
  opt_init(args, &s, &argc, argv);

  while((id = opt_next(&s)) != -1)
  {
    switch(id)
    {
      case OPT_THREADS: opt->threads = atoi(s.arg_val); break;
      case OPT_CDMIN: opt->cd_min_deferred = atoi(s.arg_val); break;
      case OPT_CDMAX: opt->cd_max_deferred = atoi(s.arg_val); break;
      case OPT_CDCONF: opt->cd_conf_group = atoi(s.arg_val); break;
      case OPT_GCINITIAL: opt->gc_initial = atoi(s.arg_val); break;
      case OPT_GCFACTOR: opt->gc_factor = atof(s.arg_val); break;
      case OPT_NOYIELD: opt->noyield = true; break;

      default: exit(-1);
    }
  }

  argv[argc] = NULL;
  return argc;
}

int pony_init(int argc, char** argv)
{
  options_t opt;
  memset(&opt, 0, sizeof(options_t));

  // Defaults.
  opt.cd_min_deferred = 4;
  opt.cd_max_deferred = 18;
  opt.cd_conf_group = 6;
  opt.gc_initial = 1 << 14;
  opt.gc_factor = 2.0f;

  argc = parse_opts(argc, argv, &opt);

#if defined(PLATFORM_IS_LINUX)
  pony_numa_init();
#endif

  heap_setinitialgc(opt.gc_initial);
  heap_setnextgcfactor(opt.gc_factor);

  pony_exitcode(0);
  scheduler_init(opt.threads, opt.noyield);
  cycle_create(opt.cd_min_deferred, opt.cd_max_deferred, opt.cd_conf_group);

  return argc;
}

int pony_start(bool library)
{
  if(!os_socket_init())
    return -1;

  if(!scheduler_start(library))
    return -1;

  if(library)
    return 0;

  return _atomic_load(&exit_code);
}

int pony_stop()
{
  scheduler_stop();
  os_socket_shutdown();

  return _atomic_load(&exit_code);
}

void pony_exitcode(int code)
{
  _atomic_store(&exit_code, code);
}
