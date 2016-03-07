#ifndef PLATFORM_THREADS_H
#define PLATFORM_THREADS_H

#include <stdbool.h>

/** Multithreading support.
 *
 */
#ifdef PLATFORM_IS_POSIX_BASED
#  include <pthread.h>
#  define pony_thread_id_t pthread_t

typedef void* (*thread_fn) (void* arg);

#  define DECLARE_THREAD_FN(NAME) void* NAME (void* arg)
#elif defined(PLATFORM_IS_WINDOWS)
#  include <process.h>
#  define pony_thread_id_t HANDLE

typedef uint32_t(__stdcall *thread_fn) (void* arg);

#  define DECLARE_THREAD_FN(NAME) uint32_t __stdcall NAME (void* arg)
#endif

#if defined(PLATFORM_IS_VISUAL_STUDIO)
#  define __pony_thread_local __declspec(thread)
#elif defined(PLATFORM_IS_CLANG_OR_GCC)
#  define __pony_thread_local __thread
#endif

#if defined(PLATFORM_IS_LINUX)
void ponyint_numa_init();

uint32_t ponyint_numa_cores();

void ponyint_numa_core_list(uint32_t* list);

uint32_t ponyint_numa_node_of_cpu(uint32_t cpu);
#endif

bool pony_thread_create(pony_thread_id_t* thread, thread_fn start, uint32_t cpu,
  void* arg);

bool pony_thread_join(pony_thread_id_t thread);

void pony_thread_detach(pony_thread_id_t thread);

pony_thread_id_t pony_thread_self();

#endif
