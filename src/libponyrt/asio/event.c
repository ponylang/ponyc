#include "event.h"
#include "../actor/actor.h"
#include "../mem/pool.h"

#include <string.h>

<<<<<<< Updated upstream
asio_event_t* asio_event_create(pony_actor_t* handler, uint32_t msg_id,
  uint64_t fd, uint32_t eflags, bool noisy, void* udata)
{
	asio_event_t* ev = POOL_ALLOC(asio_event_t);

	ev->fd = fd;
	ev->eflags = eflags;
	ev->owner = handler;
	ev->msg_id = msg_id;
	ev->noisy = noisy;
	ev->udata = udata;

	pony_gc_send();
	pony_traceactor(ev->owner);
	pony_send_done();

	return ev;
}

void asio_event_dtor(asio_event_t* ev)
{
	pony_gc_recv();
	pony_traceactor(ev->owner);
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
