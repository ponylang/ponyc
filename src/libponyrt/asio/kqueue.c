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

	b->kq = kqueue();

	if(b->kq == -1)
	{
		POOL_FREE(asio_backend_t, b);
		return NULL;
	}

	pipe(b->wakeup);

	struct kevent new_event;
	EV_SET(&new_event, b->wakeup[0], EVFILT_READ, EV_ADD, 0, 0, NULL);

	struct timespec t = {0, 0};
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
		int count = kevent(b->kq, NULL, 0, b->fired, MAX_EVENTS, NULL);

		for(int i = 0; i < count; i++)
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

			switch(ev->filter)
			{
				case EVFILT_READ:
					asio_event_send(pev, ASIO_READ);
					break;

				case EVFILT_WRITE:
					asio_event_send(pev, ASIO_WRITE);
					break;

				default: {}
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

	struct kevent event[2];
	int i = 0;

  // EV_CLEAR enforces edge triggered behaviour.
	if(ev->eflags & ASIO_READ)
	{
		EV_SET(&event[i], ev->fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, ev);
		i++;
	}

	if(ev->eflags & ASIO_WRITE)
	{
		EV_SET(&event[i], ev->fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, ev);
		i++;
	}

	struct timespec t = {0, 0};
	kevent(b->kq, event, i, NULL, 0, &t);
}

void asio_event_unsubscribe(asio_event_t* ev)
{
	asio_backend_t* b = asio_get_backend();

	if(ev->noisy)
		asio_noisy_remove();

	struct kevent event[2];
	int i = 0;

	if(ev->eflags & ASIO_READ)
	{
		EV_SET(&event[i], ev->fd, EVFILT_READ, EV_DELETE, 0, 0, ev);
		i++;
	}

	if(ev->eflags & ASIO_WRITE)
	{
		EV_SET(&event[i], ev->fd, EVFILT_WRITE, EV_DELETE, 0, 0, ev);
		i++;
	}

	struct timespec t = {0, 0};
	kevent(b->kq, event, i, NULL, 0, &t);

	asio_event_dtor(ev);
}

#endif
