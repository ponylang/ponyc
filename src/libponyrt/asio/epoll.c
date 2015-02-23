#include "asio.h"
#include "event.h"
#ifdef ASIO_USE_EPOLL

#include "../actor/messageq.h"
#include "../mem/pool.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
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
  ep.events = EPOLLIN | EPOLLRDHUP | EPOLLET;

  epoll_ctl(b->epfd, EPOLL_CTL_ADD, b->wakeup, &ep);

  return b;
}

void asio_backend_terminate(asio_backend_t* b)
{
  eventfd_write(b->wakeup, 1);
}

DECLARE_THREAD_FN(asio_backend_dispatch)
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
      uint32_t flags = 0;

      if(ev->flags & ASIO_READ)
      {
        if(ep->events & (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR))
          flags |= ASIO_READ;
      }

      if(ev->flags & ASIO_WRITE)
      {
        if(ep->events & EPOLLOUT)
          flags |= ASIO_WRITE;
      }

      if(ev->flags & ASIO_TIMER)
      {
        if(ep->events & (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR))
        {
          uint64_t missed;
          ssize_t rc = read((int)ev->data, &missed, sizeof(uint64_t));
          (void)rc;
          flags |= ASIO_TIMER;
        }
      }

      if(flags != 0)
        asio_event_send(ev, flags);
    }

    handle_queue(b);
  }

  messageq_destroy(&b->q);
  POOL_FREE(asio_backend_t, b);
  return NULL;
}

static void timer_set_nsec(int fd, uint64_t nsec)
{
  struct itimerspec ts;

  ts.it_interval.tv_sec = 0;
  ts.it_interval.tv_nsec = 0;
  ts.it_value.tv_sec = nsec / 1000000000;
  ts.it_value.tv_nsec = nsec - (ts.it_value.tv_sec * 1000000000);

  timerfd_settime(fd, 0, &ts, NULL);
}

void asio_event_subscribe(asio_event_t* ev)
{
  asio_backend_t* b = asio_get_backend();

  if(ev->noisy)
    asio_noisy_add();

  struct epoll_event ep;
  ep.data.ptr = ev;
  ep.events = EPOLLRDHUP | EPOLLET;

  if(ev->flags & ASIO_READ)
    ep.events |= EPOLLIN;

  if(ev->flags & ASIO_WRITE)
    ep.events |= EPOLLOUT;

  if(ev->flags & ASIO_TIMER)
  {
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    timer_set_nsec(fd, ev->data);
    ev->data = fd;
    ep.events |= EPOLLIN;
  }

  epoll_ctl(b->epfd, EPOLL_CTL_ADD, (int)ev->data, &ep);
}

void asio_event_update(asio_event_t* ev, uintptr_t data)
{
  if(ev->flags & ASIO_TIMER)
  {
    timer_set_nsec((int)ev->data, data);
  }
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

  if(ev->flags & ASIO_TIMER)
  {
    if(ev->data != (uintptr_t)-1)
    {
      close((int)ev->data);
      ev->data = -1;
    }
  }

  asio_msg_t* msg = (asio_msg_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(asio_msg_t)), 0);
  msg->event = ev;
  msg->flags = 0;
  messageq_push(&b->q, (pony_msg_t*)msg);

  ev->flags = 0;
}

#endif
