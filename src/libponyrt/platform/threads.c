#ifdef __linux__
#define _GNU_SOURCE
#endif
#include <platform.h>

#if defined(PLATFORM_IS_LINUX)
#ifdef USE_NUMA
  #include <numa.h>
#endif
#include <sched.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif

bool pony_thread_create(pony_thread_id_t* thread, thread_fn start, uint32_t cpu,
  void* arg)
{
  (void)cpu;

#if defined(PLATFORM_IS_WINDOWS)
  uintptr_t p = _beginthreadex(NULL, 0, start, arg, 0, NULL);

  if(!p)
    return false;

  *thread = (HANDLE)p;
#elif defined(PLATFORM_IS_LINUX)
  pthread_attr_t attr;
  pthread_attr_init(&attr);

  if(cpu != (uint32_t)-1)
  {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);

    pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &set);
  }

#ifdef USE_NUMA
  if(numa_available() != -1)
  {
    struct rlimit limit;

    if(getrlimit(RLIMIT_STACK, &limit) == 0)
    {
      int node = numa_node_of_cpu(cpu);
      void* stack = numa_alloc_onnode(limit.rlim_cur, node);
      pthread_attr_setstack(&attr, stack, limit.rlim_cur);
    }
  }
#endif

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
