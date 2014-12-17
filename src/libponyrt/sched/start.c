#include "scheduler.h"
#include "../gc/cycle.h"
#include "../dist/dist.h"
#include <string.h>
#include <stdlib.h>

#ifdef PLATFORM_IS_WINDOWS
#include <Winsock2.h>
#endif

typedef struct options_t
{
  // concurrent options
  uint32_t threads;

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

  return argc;
}

int pony_init(int argc, char** argv)
{
  options_t opt;
  memset(&opt, 0, sizeof(options_t));
  argc = parse_opts(argc, argv, &opt);

#ifdef PLATFORM_IS_WINDOWS
  WORD ver = MAKEWORD(2, 2);
  WSADATA data;

  WSAStartup(ver, &data);
#endif

  pony_exitcode(0);
  scheduler_init(opt.threads, opt.forcecd);
  cycle_create();

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

int pony_start(pony_termination_t termination)
{
  if(!scheduler_start(termination))
    return -1;

  return __pony_atomic_load_n(&exit_code, PONY_ATOMIC_ACQUIRE,
    PONY_ATOMIC_NO_TYPE);
}

int pony_stop()
{
  scheduler_stop();

  return __pony_atomic_load_n(&exit_code, PONY_ATOMIC_ACQUIRE,
    PONY_ATOMIC_NO_TYPE);
}

void pony_exitcode(int code)
{
  __pony_atomic_store_n(&exit_code, code, PONY_ATOMIC_RELEASE,
    PONY_ATOMIC_NO_TYPE);
}
