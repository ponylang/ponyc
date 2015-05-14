#include "scheduler.h"
#include "../mem/heap.h"
#include "../gc/cycle.h"
#include "../dist/dist.h"
#include "../lang/socket.h"
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
  float gc_factor;
  bool mpmcq;
  bool noyield;

  // distributed options
  bool distrib;
  bool master;
  uint32_t child_count;
  char* port;
  char* parent_host;
  char* parent_port;

  // debugging options
  bool forcecd;
} options_t;

// global data
static int exit_code;

static int parse_opts(int argc, char** argv, options_t* opt)
{
  int remove = 0;

  for(int i = 0; i < argc; i++)
  {
    if(!strcmp(argv[i], "--ponythreads"))
    {
      remove++;

      if(i < (argc - 1))
      {
        opt->threads = atoi(argv[i + 1]);
        remove++;
      }
    } else if(!strcmp(argv[i], "--ponycdmin")) {
      remove++;

      if(i < (argc - 1))
      {
        opt->cd_min_deferred = atoi(argv[i + 1]);
        remove++;
      }
    } else if(!strcmp(argv[i], "--ponycdmax")) {
      remove++;

      if(i < (argc - 1))
      {
        opt->cd_max_deferred = atoi(argv[i + 1]);
        remove++;
      }
    } else if(!strcmp(argv[i], "--ponycdconf")) {
      remove++;

      if(i < (argc - 1))
      {
        opt->cd_conf_group = atoi(argv[i + 1]);
        remove++;
      }
    } else if(!strcmp(argv[i], "--ponygcinitial")) {
      remove++;

      if(i < (argc - 1))
      {
        opt->gc_initial = atoi(argv[i + 1]);
        remove++;
      }
    } else if(!strcmp(argv[i], "--ponygcfactor")) {
      remove++;

      if(i < (argc - 1))
      {
        opt->gc_factor = (float)atof(argv[i + 1]);
        remove++;
      }
    } else if(!strcmp(argv[i], "--ponysched")) {
      remove++;

      if(i < (argc - 1))
      {
        if(!strcmp(argv[i + 1], "coop"))
          opt->mpmcq = false;
        else if(!strcmp(argv[i + 1], "mpmcq"))
          opt->mpmcq = true;

        remove++;
      }
    } else if(!strcmp(argv[i], "--ponynoyield")) {
      remove++;
      opt->noyield = true;
    } else if(!strcmp(argv[i], "--ponyforcecd")) {
      remove++;
      opt->forcecd = true;
    } else if(!strcmp(argv[i], "--ponydistrib")) {
      remove++;
      opt->distrib = true;
    } else if(!strcmp(argv[i], "--ponymaster")) {
      remove++;
      opt->master = true;
    } else if(!strcmp(argv[i], "--ponylisten")) {
      remove++;

      if(i < (argc - 1))
      {
        opt->port = argv[i + 1];
        remove++;
      }
    } else if(!strcmp(argv[i], "--ponychildren")) {
      remove++;

      if(i < (argc - 1))
      {
        opt->child_count = atoi(argv[i + 1]);
        remove++;
      }
    } else if(!strcmp(argv[i], "--ponyconnect")) {
      remove++;

      if(i < (argc - 2))
      {
        opt->parent_host = argv[i + 1];
        opt->parent_port = argv[i + 2];
        remove += 2;
      }
    }

    if(remove > 0)
    {
      argc -= remove;
      memmove(&argv[i], &argv[i + remove], (argc - i) * sizeof(char*));
      remove = 0;
      i--;
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
  opt.mpmcq = true;
  opt.cd_min_deferred = 4;
  opt.cd_max_deferred = 18;
  opt.cd_conf_group = 6;
  opt.gc_initial = 1 << 14;
  opt.gc_factor = 2.0f;

  argc = parse_opts(argc, argv, &opt);

  heap_setinitialgc(opt.gc_initial);
  heap_setnextgcfactor(opt.gc_factor);

  pony_exitcode(0);
  scheduler_init(opt.threads, opt.noyield, opt.forcecd, opt.mpmcq);
  cycle_create(opt.cd_min_deferred, opt.cd_max_deferred, opt.cd_conf_group);

#if 0
  if(opt.distrib)
  {
    dist_create(opt.port, opt.child_count, opt.master);

    if(!opt.master)
      dist_join(opt.parent_host, opt.parent_port);
  }
#endif

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

  return _atomic_load(&exit_code, __ATOMIC_ACQUIRE);
}

int pony_stop()
{
  scheduler_stop();
  os_socket_shutdown();

  return _atomic_load(&exit_code, __ATOMIC_ACQUIRE);
}

void pony_exitcode(int code)
{
  _atomic_store(&exit_code, code, __ATOMIC_RELEASE);
}
