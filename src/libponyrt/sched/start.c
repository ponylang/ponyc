#define PONY_WANT_ATOMIC_DEFS

#include "scheduler.h"
#include "cpu.h"
#include "../mem/heap.h"
#include "../actor/actor.h"
#include "../gc/cycle.h"
#include "../gc/serialise.h"
#include "../lang/process.h"
#include "../lang/socket.h"
#include "../options/options.h"
#include "ponyassert.h"
#include <dtrace.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef USE_VALGRIND
#include <valgrind/helgrind.h>
#endif

typedef struct options_t
{
  // concurrent options
  uint32_t threads;
  uint32_t min_threads;
  bool noscale;
  uint32_t thread_suspend_threshold;
  uint32_t cd_detect_interval;
  size_t gc_initial;
  double gc_factor;
  bool noyield;
  bool noblock;
  bool pin;
  bool pinasio;
  bool version;
  bool ponyhelp;
} options_t;

typedef enum running_kind_t
{
  NOT_RUNNING,
  RUNNING_DEFAULT,
  RUNNING_LIBRARY
} running_kind_t;

// global data
static PONY_ATOMIC(bool) initialised;
static PONY_ATOMIC(running_kind_t) running;
static PONY_ATOMIC(int) rt_exit_code;

static pony_language_features_init_t language_init;

enum
{
  OPT_MAXTHREADS,
  OPT_MINTHREADS,
  OPT_NOSCALE,
  OPT_SUSPENDTHRESHOLD,
  OPT_CDINTERVAL,
  OPT_GCINITIAL,
  OPT_GCFACTOR,
  OPT_NOYIELD,
  OPT_NOBLOCK,
  OPT_PIN,
  OPT_PINASIO,
  OPT_VERSION,
  OPT_PONYHELP
};

static opt_arg_t args[] =
{
  {"ponymaxthreads", 0, OPT_ARG_REQUIRED, OPT_MAXTHREADS},
  {"ponyminthreads", 0, OPT_ARG_REQUIRED, OPT_MINTHREADS},
  {"ponynoscale", 0, OPT_ARG_NONE, OPT_NOSCALE},
  {"ponysuspendthreshold", 0, OPT_ARG_REQUIRED, OPT_SUSPENDTHRESHOLD},
  {"ponycdinterval", 0, OPT_ARG_REQUIRED, OPT_CDINTERVAL},
  {"ponygcinitial", 0, OPT_ARG_REQUIRED, OPT_GCINITIAL},
  {"ponygcfactor", 0, OPT_ARG_REQUIRED, OPT_GCFACTOR},
  {"ponynoyield", 0, OPT_ARG_NONE, OPT_NOYIELD},
  {"ponynoblock", 0, OPT_ARG_NONE, OPT_NOBLOCK},
  {"ponypin", 0, OPT_ARG_NONE, OPT_PIN},
  {"ponypinasio", 0, OPT_ARG_NONE, OPT_PINASIO},
  {"ponyversion", 0, OPT_ARG_NONE, OPT_VERSION},
  {"ponyhelp", 0, OPT_ARG_NONE, OPT_PONYHELP},

  OPT_ARGS_FINISH
};

#ifdef __cplusplus
extern "C" {
#endif
void Main_runtime_override_defaults_oo(options_t* opt);
#ifdef __cplusplus
}
#endif

static const char* arg_name(const int id) {
  return args[id].long_opt;
}

static void err_out(int id, const char* msg) {
  printf("--%s %s\n", arg_name(id), msg);
  exit(255);
}

static int parse_uint(uint32_t* target, int min, const char *value) {
  int v = atoi(value);
  if (v < (min < 0 ? 0 : min)) {
    return 1;
  }
  *target = v;
  return 0;
} 

static int parse_size(size_t* target, int min, const char *value) {
  int v = atoi(value);
  if (v < (min < 0 ? 0 : min)) {
    return 1;
  }
  *target = v;
  return 0;
} 

static int parse_udouble(double* target, double min, const char *value) {
  double v = atof(value);
  if (v < (min < 0 ? 0 : min)) {
    return 1;
  }
  *target = v;
  return 0;
} 

static int parse_opts(int argc, char** argv, options_t* opt)
{
  opt_state_t s;
  int id;
  ponyint_opt_init(args, &s, &argc, argv);
  bool minthreads_set = false;

  while((id = ponyint_opt_next(&s)) != -1)
  {
    switch(id)
    {
      case OPT_MAXTHREADS: if(parse_uint(&opt->threads, 1, s.arg_val)) err_out(id, "can't be less than 1"); break;
      case OPT_MINTHREADS: if(parse_uint(&opt->min_threads, 0, s.arg_val)) err_out(id, "can't be less than 0"); minthreads_set = true; break;
      case OPT_NOSCALE: opt->noscale= true; break;
      case OPT_SUSPENDTHRESHOLD: if(parse_uint(&opt->thread_suspend_threshold, 0, s.arg_val)) err_out(id, "can't be less than 0"); break;
      case OPT_CDINTERVAL: if(parse_uint(&opt->cd_detect_interval, 0, s.arg_val)) err_out(id, "can't be less than 0"); break;
      case OPT_GCINITIAL: if(parse_size(&opt->gc_initial, 0, s.arg_val)) err_out(id, "can't be less than 0"); break;
      case OPT_GCFACTOR: if(parse_udouble(&opt->gc_factor, 1.0, s.arg_val)) err_out(id, "can't be less than 1.0"); break;
      case OPT_NOYIELD: opt->noyield = true; break;
      case OPT_NOBLOCK: opt->noblock = true; break;
      case OPT_PIN: opt->pin = true; break;
      case OPT_PINASIO: opt->pinasio = true; break;
      case OPT_VERSION: opt->version = true; break;
      case OPT_PONYHELP: opt->ponyhelp = true; break;

      case -2:
        // an error message has been printed by ponyint_opt_next
        exit(-1);
        break;

      default:
        exit(-1);
        break;
    }
  }

  if (opt->noscale)
  {
    if (minthreads_set)
    {
      printf("--%s & --%s are mutually exclusive\n", arg_name(OPT_MINTHREADS), arg_name(OPT_NOSCALE));
      exit(-1);
    }
    opt->min_threads = opt->threads;
  }

  argv[argc] = NULL;
  return argc;
}

PONY_API int pony_init(int argc, char** argv)
{
  bool prev_init = atomic_exchange_explicit(&initialised, true,
    memory_order_relaxed);
  (void)prev_init;
  pony_assert(!prev_init);
  pony_assert(
    atomic_load_explicit(&running, memory_order_relaxed) == NOT_RUNNING);

  atomic_thread_fence(memory_order_acquire);
#ifdef USE_VALGRIND
  ANNOTATE_HAPPENS_AFTER(&initialised);
  ANNOTATE_HAPPENS_AFTER(&running);
#endif

  DTRACE0(RT_INIT);
  options_t opt;
  memset(&opt, 0, sizeof(options_t));

  // Defaults.
  opt.min_threads = 0;
  opt.cd_detect_interval = 100;
  opt.gc_initial = 14;
  opt.gc_factor = 2.0f;
  opt.pin = false;

  pony_register_thread();

  // Allow override via bare function on Main actor
  Main_runtime_override_defaults_oo(&opt);

  argc = parse_opts(argc, argv, &opt);

  if (opt.ponyhelp) {
    printf("%s", PONYRT_HELP);
    exit(0);
  }

  if (opt.version) {
#ifdef _MSC_VER
        printf("%s %d\n", PONY_VERSION_STR, _MSC_VER);
#else
        printf("%s\n", PONY_VERSION_STR);
#endif
    exit(0);
  }

  ponyint_cpu_init();

  if (opt.threads == 0) {
    opt.threads = ponyint_cpu_count();
  }
  else if (opt.threads > ponyint_cpu_count())
  {
    printf("Can't have --%s > physical cores, the number of threads you'd be running with (%u > %u)\n", arg_name(OPT_MAXTHREADS), opt.threads, ponyint_cpu_count());
    exit(-1);
  }

  if (opt.min_threads > opt.threads)
  {
    printf("Can't have --%s > --%s (%u > %u)\n", arg_name(OPT_MINTHREADS), arg_name(OPT_MAXTHREADS), opt.min_threads, opt.threads);
    exit(-1);
  }

  ponyint_heap_setinitialgc(opt.gc_initial);
  ponyint_heap_setnextgcfactor(opt.gc_factor);
  ponyint_actor_setnoblock(opt.noblock);

  pony_exitcode(0);

  pony_ctx_t* ctx = ponyint_sched_init(opt.threads, opt.noyield, opt.pin,
    opt.pinasio, opt.min_threads, opt.thread_suspend_threshold);

  ponyint_cycle_create(ctx, opt.cd_detect_interval);

  return argc;
}

PONY_API bool pony_start(bool library, int* exit_code,
  const pony_language_features_init_t* language_features)
{
  pony_assert(atomic_load_explicit(&initialised, memory_order_relaxed));

  // Set to RUNNING_DEFAULT even if library is true so that pony_stop() isn't
  // callable until the runtime has actually started.
  running_kind_t prev_running = atomic_exchange_explicit(&running,
    RUNNING_DEFAULT, memory_order_relaxed);
  (void)prev_running;
  pony_assert(prev_running == NOT_RUNNING);

  if(language_features != NULL)
  {
    memcpy(&language_init, language_features,
      sizeof(pony_language_features_init_t));

    if(language_init.init_network && !ponyint_os_sockets_init())
    {
      atomic_store_explicit(&running, NOT_RUNNING, memory_order_relaxed);
      return false;
    }

    if(language_init.init_serialisation &&
      !ponyint_serialise_setup(language_init.descriptor_table,
        language_init.descriptor_table_size))
    {
      atomic_store_explicit(&running, NOT_RUNNING, memory_order_relaxed);
      return false;
    }
  } else {
    memset(&language_init, 0, sizeof(pony_language_features_init_t));
  }

  if(!ponyint_sched_start(library))
  {
    atomic_store_explicit(&running, NOT_RUNNING, memory_order_relaxed);
    return false;
  }

  if(library)
  {
#ifdef USE_VALGRIND
    ANNOTATE_HAPPENS_BEFORE(&running);
#endif
    atomic_store_explicit(&running, RUNNING_LIBRARY, memory_order_release);
    return true;
  }

  if(language_init.init_network)
    ponyint_os_sockets_final();

  int ec = pony_get_exitcode();
#ifdef USE_VALGRIND
  ANNOTATE_HAPPENS_BEFORE(&initialised);
  ANNOTATE_HAPPENS_BEFORE(&running);
#endif
  atomic_thread_fence(memory_order_acq_rel);
  atomic_store_explicit(&initialised, false, memory_order_relaxed);
  atomic_store_explicit(&running, NOT_RUNNING, memory_order_relaxed);

  if(exit_code != NULL)
    *exit_code = ec;

  return true;
}

PONY_API int pony_stop()
{
  pony_assert(atomic_load_explicit(&initialised, memory_order_relaxed));

  running_kind_t loc_running = atomic_load_explicit(&running,
    memory_order_acquire);
#ifdef USE_VALGRIND
  ANNOTATE_HAPPENS_AFTER(&running);
#endif
  (void)loc_running;
  pony_assert(loc_running == RUNNING_LIBRARY);

  ponyint_sched_stop();

  if(language_init.init_network)
    ponyint_os_sockets_final();

  int ec = pony_get_exitcode();
#ifdef USE_VALGRIND
  ANNOTATE_HAPPENS_BEFORE(&initialised);
  ANNOTATE_HAPPENS_BEFORE(&running);
#endif
  atomic_thread_fence(memory_order_acq_rel);
  atomic_store_explicit(&initialised, false, memory_order_relaxed);
  atomic_store_explicit(&running, NOT_RUNNING, memory_order_relaxed);
  return ec;
}

PONY_API void pony_exitcode(int code)
{
  atomic_store_explicit(&rt_exit_code, code, memory_order_relaxed);
}

PONY_API int pony_get_exitcode()
{
  return atomic_load_explicit(&rt_exit_code, memory_order_relaxed);
}
