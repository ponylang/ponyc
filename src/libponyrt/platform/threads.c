#ifdef __linux__
#define _GNU_SOURCE
#endif
#include <platform.h>

#if defined(PLATFORM_IS_LINUX) || defined(PLATFORM_IS_FREEBSD)
#include <sched.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#endif

#if defined(PLATFORM_IS_FREEBSD)
#include <pthread_np.h>
#include <sys/cpuset.h>
typedef cpuset_t cpu_set_t;
#endif

#if defined(PLATFORM_IS_LINUX)

#include <dlfcn.h>
#include <stdio.h>

struct bitmask
{
  unsigned long size;
  unsigned long *maskp;
};

static int (*_numa_available)();
static int (*_numa_num_task_cpus)();
static void (*_numa_set_localalloc)();
static int (*_numa_node_of_cpu)(int cpu);
static void* (*_numa_alloc_onnode)(size_t size, int node);

static void* (*_numa_alloc)(size_t size);
static void (*_numa_free)(void* mem, size_t size);

static struct bitmask** _numa_all_nodes_ptr;
static struct bitmask* (*_numa_allocate_cpumask)();
static void (*_numa_bitmask_free)(struct bitmask*);
static int (*_numa_node_to_cpus)(int, struct bitmask*);
static unsigned int (*_numa_bitmask_weight)(const struct bitmask *);
static int (*_numa_bitmask_isbitset)(const struct bitmask*, unsigned int);

static bool use_numa = false;

#define LOAD_SYMBOL(sym) \
{ \
  typedef typeof(_##sym) f; \
  _##sym = (f)dlsym(lib, #sym); \
  err += (_##sym == NULL); \
}

void pony_numa_init()
{
  void* lib = dlopen("libnuma.so.1", RTLD_LAZY);
  int err = 0;

  if(lib == NULL)
    return;

  LOAD_SYMBOL(numa_available);
  LOAD_SYMBOL(numa_num_task_cpus);
  LOAD_SYMBOL(numa_set_localalloc);
  LOAD_SYMBOL(numa_node_of_cpu);
  LOAD_SYMBOL(numa_alloc_onnode);

  LOAD_SYMBOL(numa_alloc);
  LOAD_SYMBOL(numa_free);

  LOAD_SYMBOL(numa_all_nodes_ptr);
  LOAD_SYMBOL(numa_allocate_cpumask);
  LOAD_SYMBOL(numa_bitmask_free);
  LOAD_SYMBOL(numa_node_to_cpus);
  LOAD_SYMBOL(numa_bitmask_weight);
  LOAD_SYMBOL(numa_bitmask_isbitset);

  if(err != 0)
    return;

  if(_numa_available() == -1)
    return;

  use_numa = true;
  _numa_set_localalloc();
}

uint32_t pony_numa_cores()
{
  if(use_numa)
    return _numa_num_task_cpus();

  return 0;
}

void pony_numa_core_list(uint32_t* list)
{
  if(!use_numa)
    return;

  uint32_t nodes = _numa_bitmask_weight(*_numa_all_nodes_ptr);
  struct bitmask* cpu = _numa_allocate_cpumask();
  uint32_t node_count = 0;
  uint32_t index = 0;

  for(uint32_t i = 0; node_count < nodes; i++)
  {
    if(!_numa_bitmask_isbitset(*_numa_all_nodes_ptr, i))
      continue;

    node_count++;
    _numa_node_to_cpus(i, cpu);

    uint32_t cpus = _numa_bitmask_weight(cpu);
    uint32_t cpu_count = 0;

    for(uint32_t j = 0; cpu_count < cpus; j++)
    {
      if(!_numa_bitmask_isbitset(cpu, j))
        continue;

      cpu_count++;
      list[index++] = j;
    }
  }

  _numa_bitmask_free(cpu);
}

uint32_t pony_numa_node_of_cpu(uint32_t cpu)
{
  if(use_numa)
    return _numa_node_of_cpu(cpu);

  return 0;
}

#endif

bool pony_thread_create(pony_thread_id_t* thread, thread_fn start,
  uint32_t cpu, void* arg)
{
  (void)cpu;

#if defined(PLATFORM_IS_WINDOWS)
  uintptr_t p = _beginthreadex(NULL, 0, start, arg, 0, NULL);

  if(!p)
    return false;

  *thread = (HANDLE)p;
#elif defined(PLATFORM_IS_LINUX) || defined(PLATFORM_IS_FREEBSD)
  pthread_attr_t attr;
  pthread_attr_init(&attr);

  if(cpu != (uint32_t)-1)
  {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);

    pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &set);

#if defined(PLATFORM_IS_LINUX)
    if(use_numa)
    {
      struct rlimit limit;

      if(getrlimit(RLIMIT_STACK, &limit) == 0)
      {
        int node = _numa_node_of_cpu(cpu);
        void* stack = _numa_alloc_onnode(limit.rlim_cur, node);
        if (stack != NULL) {
          pthread_attr_setstack(&attr, stack, limit.rlim_cur);
        }
      }
    }
#endif
  }

  if(pthread_create(thread, &attr, start, arg))
    return false;

  pthread_attr_destroy(&attr);
#else
  if(pthread_create(thread, NULL, start, arg))
    return false;
#endif
  return true;
}

bool pony_thread_join(pony_thread_id_t thread)
{
#ifdef PLATFORM_IS_WINDOWS
  WaitForSingleObject(thread, INFINITE);
  CloseHandle(thread);
#else
  if(pthread_join(thread, NULL))
    return false;
#endif

  return true;
}

void pony_thread_detach(pony_thread_id_t thread)
{
#ifndef PLATFORM_IS_WINDOWS
  pthread_detach(thread);
#endif
}

pony_thread_id_t pony_thread_self()
{
#ifdef PLATFORM_IS_WINDOWS
  return GetCurrentThread();
#else
  return pthread_self();
#endif
}
