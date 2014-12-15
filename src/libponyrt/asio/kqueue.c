#include "asio.h"
#include "event.h"
#ifdef ASIO_USE_KQUEUE

#include "../mem/pool.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <string.h>
#include <stdbool.h>

#include <unistd.h>

static struct timespec t;

struct asio_backend_t
{
	int kq;
	int wakeup[2];
	struct kevent fired[MAX_EVENTS];
};

asio_backend_t* asio_backend_init()
{
	asio_backend_t* b = POOL_ALLOC(asio_backend_t);
	memset(b, 0, sizeof(asio_backend_t));

	struct kevent new_event;

	b->kq = kqueue();

	if(b->kq == -1)
	{
		POOL_FREE(asio_backend_t, b);
		return NULL;
	}

	pipe(&b->wakeup[0]);

	EV_SET(&new_event, b->wakeup[0], EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
	kevent(b->kq, &new_event, 1, NULL, 0, &t);

	return b;
}

void asio_backend_terminate(asio_backend_t* b)
{
	char c = '\0';
	write(b->wakeup[1], &c, 1);
}

DEFINE_THREAD_FN(asio_backend_dispatch,
{
	asio_backend_t* b = arg;

	while(true)
	{
		int32_t event_cnt = kevent(b->kq, NULL, 0, b->fired, MAX_EVENTS, NULL);

		if(event_cnt > 0)
		{
			for(int32_t i = 0; i < event_cnt; i++)
			{
				struct kevent* ev = &(b->fired[i]);

				if((ev->ident == (uintptr_t)b->wakeup[0]) &&
					(ev->filter == EVFILT_READ))
				{
					close(b->kq);
					close(b->wakeup[0]);
					close(b->wakeup[1]);
					break;
				}

				asio_event_t* pev = ev->udata;

				//TODO FILES etc.
				uint32_t aflags = (ev->filter & EVFILT_READ ? ASIO_READABLE : 0)
								| (ev->filter & EVFILT_WRITE ? ASIO_WRITABLE : 0)
								| (ev->flags & EV_EOF ? ASIO_PEER_SHUTDOWN : 0)
								| (ev->flags & EV_ERROR ? ASIO_ERROR : 0);

				asio_event_send(pev, aflags);
			}
		}
	}

	POOL_FREE(asio_backend_t, b);

	return NULL;
});

void asio_event_subscribe(asio_event_t* ev)
{
	asio_backend_t* b = asio_get_backend();

	if(ev->noisy)
		asio_noisy_add();

	struct kevent new_event;

	uint32_t oldmask = (ev->eflags >> 16);
	uint32_t mask = (ev->eflags & ASIO_FILT_READ ? EVFILT_READ : 0)
								| (ev->eflags & ASIO_FILT_WRITE ? EVFILT_WRITE : 0);

	ev->eflags |= (ev->eflags << 16);

	if(oldmask == mask)
		return;

  //EV_CLEAR enforces edge triggered behaviour
	EV_SET(&new_event, ev->fd, mask, EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, ev);
	kevent(b->kq, &new_event, 1, NULL, 0, &t);
}

void asio_event_unsubscribe(asio_event_t* ev)
{
	asio_backend_t* b = asio_get_backend();

	if(ev->noisy)
		asio_noisy_remove();

	struct kevent new_event;

	EV_SET(&new_event, ev->fd, 0, EV_DELETE, 0, 0, 0);
	kevent(b->kq, &new_event, 1, NULL, 0, &t);

	asio_event_dtor(ev);
}

#endif
