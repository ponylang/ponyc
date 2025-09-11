#include "event.h"
#include "asio.h"
#include "../actor/actor.h"
#include "../mem/pool.h"
#include "../sched/scheduler.h"
#include "ponyassert.h"
#include <string.h>

PONY_API asio_event_t* pony_asio_event_create(pony_actor_t* owner, int fd,
  uint32_t flags, uint64_t nsec, bool noisy)
{
  if((flags == ASIO_DISPOSABLE) || (flags == ASIO_DESTROYED))
    return NULL;

  pony_type_t* type = *(pony_type_t**)owner;
  uint32_t msg_id = type->event_notify;

  if(msg_id == (uint32_t)-1)
    return NULL;

  asio_event_t* ev = POOL_ALLOC(asio_event_t);

  ev->magic = ev;
  ev->owner = owner;
  ev->msg_id = msg_id;
  ev->fd = fd;
  ev->flags = flags;
  ev->noisy = noisy;
  ev->nsec = nsec;
  ev->writeable = false;
  ev->readable = false;

  owner->live_asio_events = owner->live_asio_events + 1;

  // The event is effectively being sent to another thread, so mark it here.
  pony_ctx_t* ctx = pony_ctx();
  pony_gc_send(ctx);
  pony_traceknown(ctx, owner, type, PONY_TRACE_OPAQUE);
  pony_send_done(ctx);

  pony_asio_event_subscribe(ev);
  return ev;
}

PONY_API void pony_asio_event_destroy(asio_event_t* ev)
{
  if((ev == NULL) || (ev->magic != ev) || (ev->flags != ASIO_DISPOSABLE))
  {
    pony_assert(0);
    return;
  }

  if(ev->noisy)
  {
    uint64_t old_count = ponyint_asio_noisy_remove();
    // tell scheduler threads that asio has no noisy actors
    // if the old_count was 1
    if (old_count == 1)
      ponyint_sched_unnoisy_asio(pony_scheduler_index());

    ev->noisy = false;
  }

  ev->flags = ASIO_DESTROYED;

  // When we let go of an event, we treat it as if we had received it back from
  // the asio thread.
  pony_ctx_t* ctx = pony_ctx();
  pony_gc_recv(ctx);
  pony_traceunknown(ctx, ev->owner, PONY_TRACE_OPAQUE);
  pony_recv_done(ctx);

  pony_assert(ev->owner->live_asio_events > 0);
  ev->owner->live_asio_events = ev->owner->live_asio_events - 1;

  POOL_FREE(asio_event_t, ev);
}

PONY_API int pony_asio_event_fd(asio_event_t* ev)
{
  if(ev == NULL)
    return -1;

  return ev->fd;
}

PONY_API bool pony_asio_event_get_disposable(asio_event_t* ev)
{
  if(ev == NULL)
    return false;

  return (ev->flags == ASIO_DISPOSABLE);
}

PONY_API bool pony_asio_event_get_writeable(asio_event_t* ev)
{
  if(ev == NULL)
    return false;

  return ev->writeable;
}

PONY_API void pony_asio_event_set_writeable(asio_event_t* ev, bool writeable)
{
  if(ev != NULL)
    ev->writeable = writeable;
}

PONY_API bool pony_asio_event_get_readable(asio_event_t* ev)
{
  if(ev == NULL)
    return false;

  return ev->readable;
}

PONY_API void pony_asio_event_set_readable(asio_event_t* ev, bool readable)
{
  if(ev != NULL)
    ev->readable = readable;
}

PONY_API uint64_t pony_asio_event_nsec(asio_event_t* ev)
{
  if(ev == NULL)
    return 0;

  return ev->nsec;
}

PONY_API void pony_asio_event_send(asio_event_t* ev, uint32_t flags,
  uint32_t arg)
{
#ifdef PLATFORM_IS_WINDOWS
  // On Windows, this can be called from an IOCP callback thread, which may
  // not have a pony_ctx() associated with it yet.
  // Don't call `ponyint_register_asio_thread` because that would overwrite the
  // scheduler index if this is run on a normal scheduler thread and that would
  // be not good.
  pony_register_thread();
#endif

  asio_msg_t* m = (asio_msg_t*)pony_alloc_msg(POOL_INDEX(sizeof(asio_msg_t)),
    ev->msg_id);
  m->event = ev;
  m->flags = flags;
  m->arg = arg;

  // ASIO messages technically are application messages, but they are not
  // covered by backpressure. We send the message via a mechanism that will put
  // an unscheduled actor onto the global inject queue for any actor to pick up.
  ponyint_sendv_inject(ev->owner, &m->msg);

  // maybe wake up a scheduler thread if they've all fallen asleep
  ponyint_sched_maybe_wakeup_if_all_asleep(pony_scheduler_index());
}
