#include "asio.h"
#include "event.h"
#ifdef ASIO_USE_EPOLL

#include "../mem/pool.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <string.h>
#include <stdbool.h>

struct asio_backend_t
{
	intptr_t epfd;
	intptr_t wakeup;    /* eventfd to break epoll loop */
	struct epoll_event events[MAX_EVENTS];
};

asio_backend_t* asio_backend_init()
{
	asio_backend_t* b = POOL_ALLOC(asio_backend_t);
	memset(b, 0, sizeof(asio_backend_t));

	b->epfd = epoll_create1(EPOLL_CLOEXEC);
	b->wakeup = eventfd(0, EFD_NONBLOCK);

	struct epoll_event ep;
	ep.data.fd = b->wakeup;

	epoll_ctl(b->epfd, EPOLL_CTL_ADD, b->wakeup, &ep);

	return b;
}

void asio_backend_terminate(asio_backend_t* b)
{
	eventfd_write(b->wakeup, 1);
}

DEFINE_THREAD_FN(asio_backend_dispatch,
{
	asio_backend_t* b = arg;

	struct epoll_event* ev;
	asio_event_t* pev;

	uint32_t i;
	uint32_t aflags;
	int32_t event_cnt;

	while(true)
	{
		if((event_cnt = epoll_wait(b->epfd, b->events, MAX_EVENTS, -1)) > 0)
		{
			for(i = 0; i < event_cnt; i++)
			{
				ev = &(b->events[i]);

				if(ev->data.fd == b->wakeup)
				{
					close(b->epfd);
					close(b->wakeup);

					break;
				}

				pev = ev->data.ptr;

				aflags = (ev->events & EPOLLIN ? ASIO_READABLE : 0)
								| (ev->events & EPOLLOUT ? ASIO_WRITABLE : 0)
								| (ev->events & EPOLLRDHUP ? ASIO_PEER_SHUTDOWN : 0)
								| (ev->events & EPOLLHUP ? ASIO_CLOSED_UNEXPECTEDLY : 0)
								| (ev->events & EPOLLERR ? ASIO_ERROR : 0);

				asio_event_send(pev, aflags);
			}
		}
	}

	POOL_FREE(asio_backend_t, b);

	return NULL;
})

void asio_event_subscribe(asio_event_t* ev)
{
	asio_backend_t* b = asio_get_backend();
	uint32_t oldmask = (ev->eflags >> 16);

	struct epoll_event ep;
	ep.data.ptr = ev;

	ep.events = (ev->eflags & ASIO_FILT_READ ? EPOLLIN : 0)
						| (ev->eflags & ASIO_FILT_WRITE ? EPOLLOUT : 0)
						| EPOLLET
						| EPOLLRDHUP
						| EPOLLHUP;

	ev->eflags |= (ev->eflags << 16);

	if(oldmask == ep.events)
		return;

	epoll_ctl(b->epfd, (oldmask != 0) && (oldmask != ep.events) ? EPOLL_CTL_MOD :
		EPOLL_CTL_ADD, ev->fd, &ep);
}

void asio_event_unsubscribe(asio_event_t* ev)
{
	asio_backend_t* b = asio_get_backend();

	epoll_ctl(b->epfd, EPOLL_CTL_DEL, ev->fd, NULL);

	asio_event_dtor(&ev);
}

#endif
