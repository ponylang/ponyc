#include "event.h"
#include "asio.h"
#include "../actor/actor.h"
#include "../mem/pool.h"
#include <string.h>

asio_event_t* asio_event_create(pony_actor_t* owner, uintptr_t data,
  uint32_t flags, bool noisy)
{
  if((flags == ASIO_DISPOSABLE) || (flags == ASIO_DESTROYED))
    return NULL;

  pony_type_t* type = *(pony_type_t**)owner;
  uint32_t msg_id = type->event_notify;

  if(msg_id == (uint32_t)-1)
    return NULL;

  asio_event_t* ev = POOL_ALLOC(asio_event_t);

  ev->data = data;
  ev->owner = owner;
  ev->msg_id = msg_id;
  ev->flags = flags;
  ev->noisy = noisy;

  // The event is effectively being sent to another thread, so mark it here.
  pony_gc_send();
  pony_traceactor(owner);
  pony_send_done();

  asio_event_subscribe(ev);
  return ev;
}

void asio_event_destroy(asio_event_t* ev)
{
  if((ev == NULL) || (ev->flags != ASIO_DISPOSABLE))
    return;

  ev->flags = ASIO_DESTROYED;

  // When we let go of an event, we treat it as if we had received it back from
  // the asio thread.
  pony_gc_recv();
  pony_traceactor(ev->owner);
  pony_recv_done();

  POOL_FREE(asio_event_t, ev);
}

uintptr_t asio_event_data(asio_event_t* ev)
{
  return ev->data;
}

void asio_event_setdata(asio_event_t* ev, uintptr_t data)
{
  ev->data = data;
}

void asio_event_send(asio_event_t* ev, uint32_t flags)
{
  asio_msg_t* m = (asio_msg_t*)pony_alloc_msg(POOL_INDEX(sizeof(asio_msg_t)),
    ev->msg_id);
  m->event = ev;
  m->flags = flags;

  pony_sendv(ev->owner, &m->msg);
}
