#include "scheduler.h"
#include "cpu.h"
#include "../mem/heap.h"
#include "../actor/actor.h"
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
  bool noblock;
  bool nopin;
  bool pinasio;
} options_t;

// global data
static ATOMIC_TYPE(int) exit_code;

enum
{
  OPT_THREADS,
  OPT_CDMIN,
  OPT_CDMAX,
  OPT_CDCONF,
  OPT_GCINITIAL,
  OPT_GCFACTOR,
  OPT_NOYIELD,
  OPT_NOBLOCK,
  OPT_NOPIN,
  OPT_PINASIO
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
  {"ponynoblock", 0, OPT_ARG_NONE, OPT_NOBLOCK},
  {"ponynopin", 0, OPT_ARG_NONE, OPT_NOPIN},
  {"ponypinasio", 0, OPT_ARG_NONE, OPT_PINASIO},

  OPT_ARGS_FINISH
};

static int parse_opts(int argc, char** argv, options_t* opt)
{
  opt_state_t s;
  int id;
  ponyint_opt_init(args, &s, &argc, argv);

  while((id = ponyint_opt_next(&s)) != -1)
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
      case OPT_NOBLOCK: opt->noblock = true; break;
      case OPT_NOPIN: opt->nopin = true; break;
      case OPT_PINASIO: opt->pinasio = true; break;

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
  opt.cd_max_deferred = 8;
  opt.cd_conf_group = 6;
  opt.gc_initial = 14;
  opt.gc_factor = 2.0f;

  argc = parse_opts(argc, argv, &opt);

  ponyint_cpu_init();

  ponyint_heap_setinitialgc(opt.gc_initial);
  ponyint_heap_setnextgcfactor(opt.gc_factor);
  ponyint_actor_setnoblock(opt.noblock);

  pony_exitcode(0);

  pony_ctx_t* ctx = ponyint_sched_init(opt.threads, opt.noyield, opt.nopin,
    opt.pinasio);

  ponyint_cycle_create(ctx,
    opt.cd_min_deferred, opt.cd_max_deferred, opt.cd_conf_group);

  return argc;
}

int pony_start(bool library)
{
  if(!ponyint_os_sockets_init())
    return -1;

  if(!ponyint_sched_start(library))
    return -1;

  if(library)
    return 0;

  return atomic_load_explicit(&exit_code, memory_order_relaxed);
}

int pony_stop()
{
  ponyint_sched_stop();
  ponyint_os_sockets_final();

  return atomic_load_explicit(&exit_code, memory_order_relaxed);
}

void pony_exitcode(int code)
{
  atomic_store_explicit(&exit_code, code, memory_order_relaxed);
}
