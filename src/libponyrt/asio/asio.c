#define PONY_WANT_ATOMIC_DEFS

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include "asio.h"
#include "../mem/pool.h"
#include "../sched/systematic_testing.h"

struct asio_base_t
{
  pony_thread_id_t tid;
#if defined(USE_SYSTEMATIC_TESTING)
  pony_signal_event_t sleep_object;
#endif
  asio_backend_t* backend;
  PONY_ATOMIC(uint64_t) noisy_count;
};

static asio_base_t running_base;
static uint32_t asio_cpu;

#ifdef USE_MEMTRACK
// holds only size of pthread_cond_t condition variable when using pthreads
static size_t mem_allocated;
static size_t mem_used;

/** Get the static memory used by the asio subsystem.
 */
size_t ponyint_asio_static_mem_size()
{
  return mem_used;
}

/** Get the static memory allocated by the asio subsystem.
 */
size_t ponyint_asio_static_alloc_size()
{
  return mem_allocated;
}
#endif


/** Start an asynchronous I/O event mechanism.
 *
 *  Errors are always delegated to the owning actor of an I/O subscription and
 *  never handled within the runtime system.
 *
 *  In any case (independent of the underlying backend) only one I/O dispatcher
 *  thread will be started. Since I/O events are subscribed by actors, we do
 *  not need to maintain a thread pool. Instead, I/O is processed in the
 *  context of the owning actor.
 */
asio_backend_t* ponyint_asio_get_backend()
{
  return running_base.backend;
}

pony_thread_id_t ponyint_asio_get_backend_tid()
{
  return running_base.tid;
}

#if defined(USE_SYSTEMATIC_TESTING)
pony_signal_event_t ponyint_asio_get_backend_sleep_object()
{
  return running_base.sleep_object;
}
#endif

uint32_t ponyint_asio_get_cpu()
{
  return asio_cpu;
}

void ponyint_asio_init(uint32_t cpu)
{
  asio_cpu = cpu;
  running_base.backend = ponyint_asio_backend_init();

#if defined(USE_SYSTEMATIC_TESTING)
#if defined(PLATFORM_IS_WINDOWS)
    // create wait event objects
    running_base.sleep_object = CreateEvent(NULL, FALSE, FALSE, NULL);
#elif defined(USE_SCHEDULER_SCALING_PTHREADS)
    // create pthread condition object
    running_base.sleep_object = POOL_ALLOC(pthread_cond_t);
    int ret = pthread_cond_init(running_base.sleep_object, NULL);
    if(ret != 0)
    {
      // if it failed, set `sleep_object` to `NULL` for error
      POOL_FREE(pthread_cond_t, running_base.sleep_object);
      running_base.sleep_object = NULL;
    }

#ifdef USE_MEMTRACK
    mem_used += sizeof(pthread_cond_t);
    mem_allocated += POOL_ALLOC_SIZE(pthread_cond_t);
#endif
#endif
#endif
}

bool ponyint_asio_start()
{
  // if the backend wasn't successfully initialized
  if(running_base.backend == NULL)
    return false;

#if defined(USE_SYSTEMATIC_TESTING) && defined(USE_SCHEDULER_SCALING_PTHREADS)
  // there was an error creating a wait event or a pthread condition object
  if(running_base.sleep_object == NULL)
    return false;
#endif

  if(!ponyint_thread_create(&running_base.tid, ponyint_asio_backend_dispatch,
    asio_cpu, running_base.backend))
    return false;

  return true;
}

bool ponyint_asio_stop()
{
  if(!ponyint_asio_stoppable())
    return false;

  if(running_base.backend != NULL)
  {
    ponyint_asio_backend_final(running_base.backend);

#if defined(USE_SYSTEMATIC_TESTING)
    // wait for asio thread to shut down
    while(!SYSTEMATIC_TESTING_ASIO_STOPPED())
      SYSTEMATIC_TESTING_YIELD();
#endif

    ponyint_thread_join(running_base.tid);

#if defined(USE_SCHEDULER_SCALING_PTHREADS)
#if defined(USE_SYSTEMATIC_TESTING)
    POOL_FREE(pthread_cond_t, running_base.sleep_object);
    running_base.sleep_object = NULL;
#ifdef USE_MEMTRACK
    mem_used -= sizeof(pthread_cond_t);
    mem_allocated -= POOL_ALLOC_SIZE(pthread_cond_t);
#endif
#endif
#endif

    running_base.backend = NULL;
    running_base.tid = 0;
  }

  return true;
}

bool ponyint_asio_stoppable()
{
  // can only stop if we have no noisy actors
  return atomic_load_explicit(&running_base.noisy_count, memory_order_acquire) == 0;
}

uint64_t ponyint_asio_noisy_add()
{
  return atomic_fetch_add_explicit(&running_base.noisy_count, 1, memory_order_release);
}

uint64_t ponyint_asio_noisy_remove()
{
  return atomic_fetch_sub_explicit(&running_base.noisy_count, 1, memory_order_release);
}
