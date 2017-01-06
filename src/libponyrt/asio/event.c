#include "event.h"
#include "asio.h"
#include "../actor/actor.h"
#include "../mem/pool.h"
#include <string.h>
#include <assert.h>

asio_event_t* pony_asio_event_create(pony_actor_t* owner, int fd,
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

  // The event is effectively being sent to another thread, so mark it here.
  pony_ctx_t* ctx = pony_ctx();
  pony_gc_send(ctx);
  pony_traceknown(ctx, owner, type, PONY_TRACE_OPAQUE);
  pony_send_done(ctx);

  pony_asio_event_subscribe(ev);
  return ev;
}

void pony_asio_event_destroy(asio_event_t* ev)
{
  if((ev == NULL) || (ev->magic != ev) || (ev->flags != ASIO_DISPOSABLE))
  {
    assert(0);
    return;
  }

  ev->flags = ASIO_DESTROYED;

  // When we let go of an event, we treat it as if we had received it back from
  // the asio thread.
  pony_ctx_t* ctx = pony_ctx();
  pony_gc_recv(ctx);
  pony_traceunknown(ctx, ev->owner, PONY_TRACE_OPAQUE);
  pony_recv_done(ctx);

  POOL_FREE(asio_event_t, ev);
}

int pony_asio_event_fd(asio_event_t* ev)
{
  if(ev == NULL)
    return -1;

  return ev->fd;
}

bool pony_asio_event_get_writeable(asio_event_t* ev)
{
  if(ev == NULL)
    return false;

  return ev->writeable;
}

void pony_asio_event_set_writeable(asio_event_t* ev, bool writeable)
{
  if(ev != NULL)
    ev->writeable = writeable;
}

bool pony_asio_event_get_readable(asio_event_t* ev)
{
  if(ev == NULL)
    return false;

  return ev->readable;
}

void pony_asio_event_set_readable(asio_event_t* ev, bool readable)
{
  if(ev != NULL)
    ev->readable = readable;
}

uint64_t pony_asio_event_nsec(asio_event_t* ev)
{
  if(ev == NULL)
    return 0;

  return ev->nsec;
}

void pony_asio_event_send(asio_event_t* ev, uint32_t flags, uint32_t arg)
{
  asio_msg_t* m = (asio_msg_t*)pony_alloc_msg(POOL_INDEX(sizeof(asio_msg_t)),
    ev->msg_id);
  m->event = ev;
  m->flags = flags;
  m->arg = arg;

#ifdef PLATFORM_IS_WINDOWS
  // On Windows, this can be called from an IOCP callback thread, which may
  // not have a pony_ctx() associated with it yet.
  pony_register_thread();
#endif

  pony_sendv(pony_ctx(), ev->owner, &m->msg);
}
