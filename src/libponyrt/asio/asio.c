#define PONY_WANT_ATOMIC_DEFS

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include "asio.h"

struct asio_base_t
{
  pony_thread_id_t tid;
  asio_backend_t* backend;
  PONY_ATOMIC(uint64_t) noisy_count;
};

static asio_base_t running_base;
static uint32_t asio_cpu;

#ifdef USE_RUNTIMESTATS
// The asio subsystem holds no static memory of its own; these are kept for
// interface parity with the other subsystems' runtimestats getters and report
// zero.
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
 *  thread will be started (none under systematic testing, which does not run the
 *  ASIO thread; see ponyint_asio_start). Since I/O events are subscribed by
 *  actors, we do not need to maintain a thread pool. Instead, I/O is processed
 *  in the context of the owning actor.
 */
asio_backend_t* ponyint_asio_get_backend()
{
  return running_base.backend;
}

uint32_t ponyint_asio_get_cpu()
{
  return asio_cpu;
}

void ponyint_asio_init(uint32_t cpu)
{
  asio_cpu = cpu;

#if !defined(USE_SYSTEMATIC_TESTING)
  // Under systematic testing the ASIO thread is not run and no I/O event can be
  // registered (pony_asio_event_create aborts), so the backend would never be
  // used. It is left NULL; ponyint_asio_start and ponyint_asio_stop treat a NULL
  // backend as "nothing to run / nothing to tear down".
  running_base.backend = ponyint_asio_backend_init();
#endif
}

bool ponyint_asio_start()
{
#if defined(USE_SYSTEMATIC_TESTING)
  // The ASIO thread is not run under systematic testing (see
  // pony_asio_event_create for why). The backend is not created (see
  // ponyint_asio_init) and there is no thread to start.
  return true;
#else
  // if the backend wasn't successfully initialized
  if(running_base.backend == NULL)
    return false;

  if(!ponyint_thread_create(&running_base.tid, ponyint_asio_backend_dispatch,
    asio_cpu, running_base.backend))
    return false;

  return true;
#endif
}

bool ponyint_asio_stop()
{
  if(!ponyint_asio_stoppable())
    return false;

  // Under systematic testing the backend was never created (see
  // ponyint_asio_init), so this block is skipped -- there is no ASIO thread to
  // finalize or join.
  if(running_base.backend != NULL)
  {
    ponyint_asio_backend_final(running_base.backend);

    // If we weren't able to wait on the asio
    // thread until termination, return false
    // Otherwise, we are probably setting a running
    // backend to NULL and hilarity will ensue
    if(!ponyint_thread_join(running_base.tid))
      return false;

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
