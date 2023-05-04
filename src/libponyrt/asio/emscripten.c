#define PONY_WANT_ATOMIC_DEFS

#include "asio.h"
#include "event.h"
// #define ASIO_USE_EMSCRIPTEN // TODO: Remove this define before commit.
#ifdef ASIO_USE_EMSCRIPTEN

#include "../actor/messageq.h"
#include "../mem/pool.h"
#include "../sched/cpu.h"
#include "../sched/scheduler.h"
#include "ponyassert.h"
#include <unistd.h>
#include <string.h>

#ifdef USE_VALGRIND
#include <valgrind/helgrind.h>
#endif

#define MAX_SIGNAL 128

struct asio_backend_t
{
  PONY_ATOMIC(bool) terminate;
  messageq_t q;
};

asio_backend_t* ponyint_asio_backend_init()
{
  asio_backend_t* b = POOL_ALLOC(asio_backend_t);
  memset(b, 0, sizeof(asio_backend_t));
  ponyint_messageq_init(&b->q);

  // TODO: Implement a real ASIO backend for Emscripten. This is just a stub.

  return b;
}

void ponyint_asio_backend_final(asio_backend_t* b)
{
  atomic_store_explicit(&b->terminate, true, memory_order_relaxed);
  // TODO: Implement a real ASIO backend for Emscripten. This is just a stub.
}

// Single function for resubscribing to both reads and writes for an event
PONY_API void pony_asio_event_resubscribe(asio_event_t* ev)
{
  (void)ev;
  // TODO: Implement a real ASIO backend for Emscripten. This is just a stub.
}

// Kept to maintain backwards compatibility so folks don't
// have to change their code to use `pony_asio_event_resubscribe`
// immediately
PONY_API void pony_asio_event_resubscribe_write(asio_event_t* ev)
{
  (void)ev;
  // TODO: Implement a real ASIO backend for Emscripten. This is just a stub.
}

// Kept to maintain backwards compatibility so folks don't
// have to change their code to use `pony_asio_event_resubscribe`
// immediately
PONY_API void pony_asio_event_resubscribe_read(asio_event_t* ev)
{
  (void)ev;
  // TODO: Implement a real ASIO backend for Emscripten. This is just a stub.
}

static void handle_queue(asio_backend_t* b)
{
  asio_msg_t* msg;

  while((msg = (asio_msg_t*)ponyint_thread_messageq_pop(&b->q
#ifdef USE_DYNAMIC_TRACE
    , SPECIAL_THREADID_EPOLL
#endif
    )) != NULL)
  {
    asio_event_t* ev = msg->event;

    switch(msg->flags)
    {
      case ASIO_DISPOSABLE:
        pony_asio_event_send(ev, ASIO_DISPOSABLE, 0);
        break;

      default: {}
    }
  }
}

DECLARE_THREAD_FN(ponyint_asio_backend_dispatch)
{
  ponyint_cpu_affinity(ponyint_asio_get_cpu());
  pony_register_thread();
  asio_backend_t* b = arg;
  pony_assert(b != NULL);

  while(!atomic_load_explicit(&b->terminate, memory_order_relaxed))
  {
    // TODO: Implement a real ASIO backend for Emscripten. This is just a stub.
    sleep(1);

    handle_queue(b);
  }

  ponyint_messageq_destroy(&b->q, true);
  POOL_FREE(asio_backend_t, b);
  pony_unregister_thread();
  return NULL;
}

PONY_API void pony_asio_event_subscribe(asio_event_t* ev)
{
  (void)ev;
  // TODO: Implement a real ASIO backend for Emscripten. This is just a stub.
}

PONY_API void pony_asio_event_setnsec(asio_event_t* ev, uint64_t nsec)
{
  (void)ev;
  (void)nsec;
  // TODO: Implement a real ASIO backend for Emscripten. This is just a stub.
}

PONY_API void pony_asio_event_unsubscribe(asio_event_t* ev)
{
  (void)ev;
  // TODO: Implement a real ASIO backend for Emscripten. This is just a stub.
}

#endif
