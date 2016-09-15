#include "asio.h"
#include "event.h"
#ifdef ASIO_USE_KQUEUE

#include "../actor/messageq.h"
#include "../mem/pool.h"
#include <sys/event.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <assert.h>

struct asio_backend_t
{
  int kq;
  int wakeup[2];
  messageq_t q;
};

asio_backend_t* ponyint_asio_backend_init()
{
  asio_backend_t* b = POOL_ALLOC(asio_backend_t);
  memset(b, 0, sizeof(asio_backend_t));
  ponyint_messageq_init(&b->q);

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

void ponyint_asio_backend_final(asio_backend_t* b)
{
  char c = 1;
  write(b->wakeup[1], &c, 1);
}

static void handle_queue(asio_backend_t* b)
{
  asio_msg_t* msg;

  while((msg = (asio_msg_t*)ponyint_messageq_pop(&b->q)) != NULL)
    pony_asio_event_send(msg->event, ASIO_DISPOSABLE, 0);
}

static void retry_loop(asio_backend_t* b)
{
  char c = 0;
  write(b->wakeup[1], &c, 1);
}

DECLARE_THREAD_FN(ponyint_asio_backend_dispatch)
{
  pony_register_thread();
  asio_backend_t* b = arg;
  struct kevent fired[MAX_EVENTS];

  while(b->kq != -1)
  {
    int count = kevent(b->kq, NULL, 0, fired, MAX_EVENTS, NULL);

    for(int i = 0; i < count; i++)
    {
      struct kevent* ep = &fired[i];

      if((ep->ident == (uintptr_t)b->wakeup[0]) && (ep->filter == EVFILT_READ))
      {
        char terminate;
        read(b->wakeup[0], &terminate, 1);

        if(terminate == 1)
        {
          close(b->kq);
          close(b->wakeup[0]);
          close(b->wakeup[1]);
          b->kq = -1;
        }
      } else {
        asio_event_t* ev = ep->udata;

        if(ep->flags & EV_ERROR)
          continue;

        switch(ep->filter)
        {
          case EVFILT_READ:
            pony_asio_event_send(ev, ASIO_READ, 0);
            break;

          case EVFILT_WRITE:
            if(ep->flags & EV_EOF)
            {
              pony_asio_event_send(ev, ASIO_READ | ASIO_WRITE, 0);
            } else {
              pony_asio_event_send(ev, ASIO_WRITE, 0);
            }
            break;

          case EVFILT_TIMER:
            pony_asio_event_send(ev, ASIO_TIMER, 0);
            break;

          case EVFILT_SIGNAL:
            pony_asio_event_send(ev, ASIO_SIGNAL, (uint32_t)ep->data);
            break;

          default: {}
        }
      }
    }

    handle_queue(b);
  }

  ponyint_messageq_destroy(&b->q);
  POOL_FREE(asio_backend_t, b);
  return NULL;
}

void pony_asio_event_subscribe(asio_event_t* ev)
{
  if((ev == NULL) ||
    (ev->magic != ev) ||
    (ev->flags == ASIO_DISPOSABLE) ||
    (ev->flags == ASIO_DESTROYED))
  {
    assert(0);
    return;
  }

  asio_backend_t* b = ponyint_asio_get_backend();

  if(ev->noisy)
    ponyint_asio_noisy_add();

  struct kevent event[4];
  int i = 0;

  // EV_CLEAR enforces edge triggered behaviour.
  if(ev->flags & ASIO_READ)
  {
    EV_SET(&event[i], ev->fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, ev);
    i++;
  }

  if(ev->flags & ASIO_WRITE)
  {
    EV_SET(&event[i], ev->fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, ev);
    i++;
  }

  if(ev->flags & ASIO_TIMER)
  {
#if defined(PLATFORM_IS_FREEBSD) || defined(PLATFORM_IS_DRAGONFLY)
    EV_SET(&event[i], (uintptr_t)ev, EVFILT_TIMER, EV_ADD | EV_ONESHOT,
      0, ev->nsec / 1000000, ev);
#else
    EV_SET(&event[i], (uintptr_t)ev, EVFILT_TIMER, EV_ADD | EV_ONESHOT,
      NOTE_NSECONDS, ev->nsec, ev);
#endif
    i++;
  }

  if(ev->flags & ASIO_SIGNAL)
  {
    signal((int)ev->nsec, SIG_IGN);
    EV_SET(&event[i], ev->nsec, EVFILT_SIGNAL, EV_ADD | EV_CLEAR, 0, 0, ev);
    i++;
  }

  kevent(b->kq, event, i, NULL, 0, NULL);

  if(ev->fd == STDIN_FILENO)
    retry_loop(b);
}

void pony_asio_event_setnsec(asio_event_t* ev, uint64_t nsec)
{
  if((ev == NULL) ||
    (ev->magic != ev) ||
    (ev->flags == ASIO_DISPOSABLE) ||
    (ev->flags == ASIO_DESTROYED))
  {
    assert(0);
    return;
  }

  asio_backend_t* b = ponyint_asio_get_backend();

  struct kevent event[1];
  int i = 0;

  if(ev->flags & ASIO_TIMER)
  {
    ev->nsec = nsec;

#if defined(PLATFORM_IS_FREEBSD) || defined(PLATFORM_IS_DRAGONFLY)
    EV_SET(&event[i], (uintptr_t)ev, EVFILT_TIMER, EV_ADD | EV_ONESHOT,
      0, ev->nsec / 1000000, ev);
#else
    EV_SET(&event[i], (uintptr_t)ev, EVFILT_TIMER, EV_ADD | EV_ONESHOT,
      NOTE_NSECONDS, ev->nsec, ev);
#endif
    i++;
  }

  kevent(b->kq, event, i, NULL, 0, NULL);
}

void pony_asio_event_unsubscribe(asio_event_t* ev)
{
  if((ev == NULL) ||
    (ev->magic != ev) ||
    (ev->flags == ASIO_DISPOSABLE) ||
    (ev->flags == ASIO_DESTROYED))
  {
    assert(0);
    return;
  }

  asio_backend_t* b = ponyint_asio_get_backend();

  if(ev->noisy)
  {
    ponyint_asio_noisy_remove();
    ev->noisy = false;
  }

  struct kevent event[3];
  int i = 0;

  if(ev->flags & ASIO_READ)
  {
    EV_SET(&event[i], ev->fd, EVFILT_READ, EV_DELETE, 0, 0, ev);
    i++;
  }

  if(ev->flags & ASIO_WRITE)
  {
    EV_SET(&event[i], ev->fd, EVFILT_WRITE, EV_DELETE, 0, 0, ev);
    i++;
  }

  if(ev->flags & ASIO_TIMER)
  {
    EV_SET(&event[i], (uintptr_t)ev, EVFILT_TIMER, EV_DELETE, 0, 0, ev);
    i++;
  }

  if(ev->flags & ASIO_SIGNAL)
  {
    signal((int)ev->nsec, SIG_DFL);
    EV_SET(&event[i], ev->nsec, EVFILT_SIGNAL, EV_DELETE, 0, 0, ev);
    i++;
  }

  kevent(b->kq, event, i, NULL, 0, NULL);

  ev->flags = ASIO_DISPOSABLE;

  asio_msg_t* msg = (asio_msg_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(asio_msg_t)), 0);
  msg->event = ev;
  msg->flags = ASIO_DISPOSABLE;
  ponyint_messageq_push(&b->q, (pony_msg_t*)msg);

  retry_loop(b);
}

#endif
