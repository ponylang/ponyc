#include "event.h"
#include "../actor/actor.h"
#include "../mem/pool.h"
#include <string.h>

asio_event_t* asio_event_create(pony_actor_t* owner, uint32_t msg_id,
  uintptr_t fd, uint32_t flags, bool noisy, void* udata)
{
  asio_event_t* ev = POOL_ALLOC(asio_event_t);

  ev->fd = fd;
  ev->flags = flags;
  ev->owner = owner;
  ev->msg_id = msg_id;
  ev->noisy = noisy;
  ev->udata = udata;

  // The event is effectively being sent to another thread, so mark it here.
  pony_gc_send();
  pony_traceactor(owner);

  if(udata != NULL)
    pony_traceunknown(udata);

  pony_send_done();
  return ev;
}

void asio_event_destroy(asio_event_t* ev)
{
  // When we let go of an event, we treat it as if we had received it back from
  // the asio thread.
  pony_gc_recv();
  pony_traceactor(ev->owner);

  if(ev->udata != NULL)
    pony_traceunknown(ev->udata);

  pony_recv_done();
  POOL_FREE(asio_event_t, ev);
}

void asio_event_send(asio_event_t* ev, uint32_t flags)
{
  asio_msg_t* m = (asio_msg_t*)pony_alloc_msg(0, ev->msg_id);
  m->event = ev;
  m->flags = flags;

  pony_sendv(ev->owner, &m->msg);
}
