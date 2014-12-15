#include "event.h"
#include "../actor/actor.h"
#include "../mem/pool.h"

#include <string.h>

asio_event_t* asio_event_create(int fd, uint32_t eflags, uint32_t msg_id,
	bool noisy, void* udata)
{
	asio_event_t* e = POOL_ALLOC(asio_event_t);

	e->fd = fd;
	e->eflags = eflags;
	e->msg_id = msg_id;
	e->noisy = noisy;
	e->udata = udata;

	e->owner = actor_current();

	actor_inc_rc();
	return e;
}

void asio_event_dtor(asio_event_t* ev)
{
	actor_dec_rc();
	POOL_FREE(asio_event_t, ev);
}

void asio_event_send(asio_event_t* ev, uint32_t flags)
{
	asio_msg_t* m = (asio_msg_t*)pony_alloc_msg(0, ev->msg_id);
	m->event = ev;
	m->flags = flags;

	pony_sendv(ev->owner, &m->msg);
}
