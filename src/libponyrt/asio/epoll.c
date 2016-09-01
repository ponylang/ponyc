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
#include <signal.h>
#include <stdbool.h>

#define MAX_SIGNAL 128

struct asio_backend_t
{
  int epfd;
  int wakeup;    /* eventfd to break epoll loop */
  struct epoll_event events[MAX_EVENTS];
  asio_event_t* sighandlers[MAX_SIGNAL];
  bool terminate;
  messageq_t q;
};

static void send_request(asio_event_t* ev, int req)
{
  asio_backend_t* b = ponyint_asio_get_backend();

  asio_msg_t* msg = (asio_msg_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(asio_msg_t)), 0);
  msg->event = ev;
  msg->flags = req;
  ponyint_messageq_push(&b->q, (pony_msg_t*)msg);

  eventfd_write(b->wakeup, 1);
}

static void signal_handler(int sig)
{
  if(sig >= MAX_SIGNAL)
    return;

  // Reset the signal handler.
  signal(sig, signal_handler);
  asio_backend_t* b = ponyint_asio_get_backend();
  asio_event_t* ev = b->sighandlers[sig];

  if(ev == NULL)
    return;

  eventfd_write(ev->fd, 1);
}

static void handle_queue(asio_backend_t* b)
{
  asio_msg_t* msg;

  while((msg = (asio_msg_t*)ponyint_messageq_pop(&b->q)) != NULL)
  {
    asio_event_t* ev = msg->event;

    switch(msg->flags)
    {
      case ASIO_DISPOSABLE:
        pony_asio_event_send(ev, ASIO_DISPOSABLE, 0);
        break;

      default: {}
    }
  }
}

asio_backend_t* ponyint_asio_backend_init()
{
  asio_backend_t* b = POOL_ALLOC(asio_backend_t);
  memset(b, 0, sizeof(asio_backend_t));
  ponyint_messageq_init(&b->q);

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

void ponyint_asio_backend_final(asio_backend_t* b)
{
  b->terminate = true;
  eventfd_write(b->wakeup, 1);
}

DECLARE_THREAD_FN(ponyint_asio_backend_dispatch)
{
  pony_register_thread();
  asio_backend_t* b = arg;

  while(!b->terminate)
  {
    int event_cnt = epoll_wait(b->epfd, b->events, MAX_EVENTS, -1);

    for(int i = 0; i < event_cnt; i++)
    {
      struct epoll_event* ep = &(b->events[i]);

      if(ep->data.ptr == b)
        continue;

      asio_event_t* ev = ep->data.ptr;
      uint32_t flags = 0;
      uint32_t count = 0;

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
          ssize_t rc = read(ev->fd, &missed, sizeof(uint64_t));
          (void)rc;
          flags |= ASIO_TIMER;
        }
      }

      if(ev->flags & ASIO_SIGNAL)
      {
        if(ep->events & (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR))
        {
          uint64_t missed;
          ssize_t rc = read(ev->fd, &missed, sizeof(uint64_t));
          (void)rc;
          flags |= ASIO_SIGNAL;
          count = (uint32_t)missed;
        }
      }

      if(flags != 0)
        pony_asio_event_send(ev, flags, count);
    }

    handle_queue(b);
  }

  close(b->epfd);
  close(b->wakeup);
  ponyint_messageq_destroy(&b->q);
  POOL_FREE(asio_backend_t, b);
  return NULL;
}

static void timer_set_nsec(int fd, uint64_t nsec)
{
  struct itimerspec ts;

  ts.it_interval.tv_sec = 0;
  ts.it_interval.tv_nsec = 0;
  ts.it_value.tv_sec = (time_t)(nsec / 1000000000);
  ts.it_value.tv_nsec = (long)(nsec - (ts.it_value.tv_sec * 1000000000));

  timerfd_settime(fd, 0, &ts, NULL);
}

void pony_asio_event_subscribe(asio_event_t* ev)
{
  if((ev == NULL) ||
    (ev->flags == ASIO_DISPOSABLE) ||
    (ev->flags == ASIO_DESTROYED))
    return;

  asio_backend_t* b = ponyint_asio_get_backend();

  if(ev->noisy)
    ponyint_asio_noisy_add();

  struct epoll_event ep;
  ep.data.ptr = ev;
  ep.events = EPOLLRDHUP | EPOLLET;

  if(ev->flags & ASIO_READ)
    ep.events |= EPOLLIN;

  if(ev->flags & ASIO_WRITE)
    ep.events |= EPOLLOUT;

  if(ev->flags & ASIO_TIMER)
  {
    ev->fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    timer_set_nsec(ev->fd, ev->nsec);
    ep.events |= EPOLLIN;
  }

  if(ev->flags & ASIO_SIGNAL)
  {
    int sig = (int)ev->nsec;
    asio_event_t* prev = NULL;

    if((sig < MAX_SIGNAL) && _atomic_cas(&b->sighandlers[sig], &prev, ev,
      __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
    {
      signal(sig, signal_handler);
      ev->fd = eventfd(0, EFD_NONBLOCK);
      ep.events |= EPOLLIN;
    } else {
      return;
    }
  }

  epoll_ctl(b->epfd, EPOLL_CTL_ADD, ev->fd, &ep);
}

void pony_asio_event_setnsec(asio_event_t* ev, uint64_t nsec)
{
  if((ev == NULL) ||
    (ev->flags == ASIO_DISPOSABLE) ||
    (ev->flags == ASIO_DESTROYED))
    return;

  if(ev->flags & ASIO_TIMER)
  {
    ev->nsec = nsec;
    timer_set_nsec(ev->fd, nsec);
  }
}

void pony_asio_event_unsubscribe(asio_event_t* ev)
{
  if((ev == NULL) ||
    (ev->flags == ASIO_DISPOSABLE) ||
    (ev->flags == ASIO_DESTROYED))
    return;

  asio_backend_t* b = ponyint_asio_get_backend();

  if(ev->noisy)
  {
    ponyint_asio_noisy_remove();
    ev->noisy = false;
  }

  epoll_ctl(b->epfd, EPOLL_CTL_DEL, ev->fd, NULL);

  if(ev->flags & ASIO_TIMER)
  {
    if(ev->fd != -1)
    {
      close(ev->fd);
      ev->fd = -1;
    }
  }

  if(ev->flags & ASIO_SIGNAL)
  {
    int sig = (int)ev->nsec;
    asio_event_t* prev = ev;

    if((sig < MAX_SIGNAL) && _atomic_cas(&b->sighandlers[sig], &prev, NULL,
      __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
    {
      signal(sig, SIG_DFL);
      close(ev->fd);
      ev->fd = -1;
    }
  }

  ev->flags = ASIO_DISPOSABLE;
  send_request(ev, ASIO_DISPOSABLE);
}

#endif
