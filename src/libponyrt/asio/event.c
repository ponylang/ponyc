#include "event.h"
#include "asio.h"
#include "../actor/actor.h"
#include "../mem/pool.h"
#include <string.h>
#include <assert.h>

asio_event_t* asio_event_create(pony_actor_t* owner, int fd, uint32_t flags,
  uint64_t nsec, bool noisy)
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

  // The event is effectively being sent to another thread, so mark it here.
  pony_ctx_t* ctx = pony_ctx();
  pony_gc_send(ctx);
  pony_traceactor(ctx, owner);
  pony_send_done(ctx);

  asio_event_subscribe(ev);
  return ev;
}

void asio_event_destroy(asio_event_t* ev)
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
  pony_traceactor(ctx, ev->owner);
  pony_recv_done(ctx);

  POOL_FREE(asio_event_t, ev);
}

int asio_event_fd(asio_event_t* ev)
{
  if(ev == NULL)
    return -1;

  return ev->fd;
}

uint64_t asio_event_nsec(asio_event_t* ev)
{
  if(ev == NULL)
    return 0;

  return ev->nsec;
}

void asio_event_send(asio_event_t* ev, uint32_t flags, uint32_t arg)
{
  asio_msg_t* m = (asio_msg_t*)pony_alloc_msg(POOL_INDEX(sizeof(asio_msg_t)),
    ev->msg_id);
  m->event = ev;
  m->flags = flags;
  m->arg = arg;

  pony_sendv(pony_ctx(), ev->owner, &m->msg);
}
