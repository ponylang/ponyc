#include "asio.h"
#include "event.h"
#ifdef ASIO_USE_EPOLL

#include "../mem/pool.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

struct asio_backend_t
{
  int epfd;
  int wakeup;    /* eventfd to break epoll loop */
  struct epoll_event events[MAX_EVENTS];
};

asio_backend_t* asio_backend_init()
{
  asio_backend_t* b = POOL_ALLOC(asio_backend_t);
  memset(b, 0, sizeof(asio_backend_t));

  b->epfd = epoll_create1(EPOLL_CLOEXEC);
  b->wakeup = eventfd(0, EFD_NONBLOCK);

  if(b->epfd == 0 || b->wakeup == 0)
  {
    POOL_FREE(asio_backend_t, b);
    return NULL;
  }

  struct epoll_event ep;
  ep.data.ptr = b;

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
      for(i = 0; i < (uint32_t)event_cnt; i++)
      {
        ev = &(b->events[i]);

        if(ev->data.ptr == b)
        {
          close(b->epfd);
          close(b->wakeup);
          break;
        }

        pev = ev->data.ptr;

        aflags =
          (ev->events & (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR) ?
            ASIO_READ : 0)
          | (ev->events & EPOLLOUT ? ASIO_WRITE : 0);

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

  struct epoll_event ep;
  ep.data.ptr = ev;

  ep.events = (ev->eflags & ASIO_READ ? EPOLLIN : 0)
    | (ev->eflags & ASIO_WRITE ? EPOLLOUT : 0)
    | EPOLLRDHUP | EPOLLET;

  epoll_ctl(b->epfd, EPOLL_CTL_ADD, (int)ev->fd, &ep);
}

void asio_event_unsubscribe(asio_event_t* ev)
{
  asio_backend_t* b = asio_get_backend();

  if(ev->noisy)
    asio_noisy_remove();

  epoll_ctl(b->epfd, EPOLL_CTL_DEL, (int)ev->fd, NULL);
  asio_event_dtor(ev);
}

#endif
