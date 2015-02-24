#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include "asio.h"
#include "../mem/pool.h"

struct asio_base_t
{
  pony_thread_id_t tid;
  asio_backend_t* backend;
  uint64_t noisy_count;
};

static asio_base_t running_base;

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
asio_backend_t* asio_get_backend()
{
  return running_base.backend;
}

void asio_init()
{
  running_base.backend = asio_backend_init();
}

bool asio_start()
{
  if(!pony_thread_create(&running_base.tid, asio_backend_dispatch, -1,
    running_base.backend))
    return false;

  return true;
}

bool asio_stop()
{
  if(__pony_atomic_load_n(
    &running_base.noisy_count, PONY_ATOMIC_ACQUIRE, uint64_t) > 0)
    return false;

  asio_backend_terminate(running_base.backend);
  pony_thread_join(running_base.tid);

  running_base.backend = NULL;
  running_base.tid = 0;

  return true;
}

void asio_noisy_add()
{
  __pony_atomic_fetch_add(&running_base.noisy_count, 1, PONY_ATOMIC_RELEASE,
    uint64_t);
}

void asio_noisy_remove()
{
  __pony_atomic_fetch_sub(&running_base.noisy_count, 1, PONY_ATOMIC_RELEASE,
    uint64_t);
}
