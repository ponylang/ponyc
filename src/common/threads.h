#ifndef PLATFORM_THREADS_H
#define PLATFORM_THREADS_H

#include <stdbool.h>

/** Multithreading support.
 *
 */
#ifdef PLATFORM_IS_POSIX_BASED
#  include <pthread.h>
#  include <signal.h>
#  define pony_thread_id_t pthread_t
#if defined(USE_SCHEDULER_SCALING_PTHREADS)
#  define pony_signal_event_t pthread_cond_t*
#else
#  define pony_signal_event_t uint32_t
#endif

typedef void* (*thread_fn) (void* arg);

#  define DECLARE_THREAD_FN(NAME) void* NAME (void* arg)
#elif defined(PLATFORM_IS_WINDOWS)
#  include <process.h>
#  define pony_thread_id_t HANDLE
#  define pony_signal_event_t HANDLE

typedef uint32_t(__stdcall *thread_fn) (void* arg);

#  define DECLARE_THREAD_FN(NAME) uint32_t __stdcall NAME (void* arg)
#endif

#if defined(PLATFORM_IS_VISUAL_STUDIO)
#  define __pony_thread_local __declspec(thread)
#elif defined(PLATFORM_IS_CLANG_OR_GCC)
#  define __pony_thread_local __thread
#endif

#if defined(PLATFORM_IS_LINUX)
#include <sched.h>

bool ponyint_numa_init();

uint32_t ponyint_numa_cores();

uint32_t ponyint_numa_core_list(cpu_set_t* hw_cpus, cpu_set_t* ht_cpus,
  uint32_t* list);

uint32_t ponyint_numa_node_of_cpu(uint32_t cpu);
#endif

bool ponyint_thread_create(pony_thread_id_t* thread, thread_fn start,
  uint32_t cpu, void* arg);

bool ponyint_thread_join(pony_thread_id_t thread);

void ponyint_thread_detach(pony_thread_id_t thread);

pony_thread_id_t ponyint_thread_self();

#if defined(USE_SCHEDULER_SCALING_PTHREADS)
void ponyint_thread_suspend(pony_signal_event_t signal, pthread_mutex_t* mut);
#else
void ponyint_thread_suspend(pony_signal_event_t signal);
#endif

int ponyint_thread_wake(pony_thread_id_t thread, pony_signal_event_t signal);

#endif
