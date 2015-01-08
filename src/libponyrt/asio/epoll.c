#include "asio.h"
#include "event.h"
#ifdef ASIO_USE_EPOLL

#include "../actor/messageq.h"
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
  messageq_t q;
};

static void handle_queue(asio_backend_t* b)
{
  asio_msg_t* msg;

  while((msg = (asio_msg_t*)messageq_pop(&b->q)) != NULL)
    asio_event_send(msg->event, 0);
}

asio_backend_t* asio_backend_init()
{
  asio_backend_t* b = POOL_ALLOC(asio_backend_t);
  memset(b, 0, sizeof(asio_backend_t));
  messageq_init(&b->q);

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

  while(b->epfd != -1)
  {
    int event_cnt = epoll_wait(b->epfd, b->events, MAX_EVENTS, -1);

    for(int i = 0; i < event_cnt; i++)
    {
      struct epoll_event* ep = &(b->events[i]);

      if(ep->data.ptr == b)
      {
        close(b->epfd);
        close(b->wakeup);
        b->epfd = -1;
        break;
      }

      asio_event_t* ev = ep->data.ptr;

      uint32_t flags =
        (ep->events & (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR) ?
          ASIO_READ : 0)
        | (ep->events & EPOLLOUT ? ASIO_WRITE : 0);

      asio_event_send(ev, flags);
    }

    handle_queue(b);
  }

  messageq_destroy(&b->q);
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

  ep.events = (ev->flags & ASIO_READ ? EPOLLIN : 0)
    | (ev->flags & ASIO_WRITE ? EPOLLOUT : 0)
    | EPOLLRDHUP | EPOLLET;

  epoll_ctl(b->epfd, EPOLL_CTL_ADD, (int)ev->data, &ep);
}

void asio_event_unsubscribe(asio_event_t* ev)
{
  asio_backend_t* b = asio_get_backend();

  if(ev->noisy)
  {
    asio_noisy_remove();
    ev->noisy = false;
  }

  if(ev->flags == 0)
    return;

  epoll_ctl(b->epfd, EPOLL_CTL_DEL, (int)ev->data, NULL);

  asio_msg_t* msg = (asio_msg_t*)pony_alloc_msg(0, 0);
  msg->event = ev;
  msg->flags = 0;
  messageq_push(&b->q, (pony_msg_t*)msg);

  ev->flags = 0;
}

#endif
