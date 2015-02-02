#include "asio.h"
#include "event.h"
#ifdef ASIO_USE_KQUEUE

#include "../actor/messageq.h"
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
  messageq_t q;
};

asio_backend_t* asio_backend_init()
{
  asio_backend_t* b = POOL_ALLOC(asio_backend_t);
  memset(b, 0, sizeof(asio_backend_t));
  messageq_init(&b->q);

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
  char c = 1;
  write(b->wakeup[1], &c, 1);
}

static void handle_queue(asio_backend_t* b)
{
  asio_msg_t* msg;

  while((msg = (asio_msg_t*)messageq_pop(&b->q)) != NULL)
    asio_event_send(msg->event, 0);
}

static void retry_loop(asio_backend_t* b)
{
  char c = 0;
  write(b->wakeup[1], &c, 1);
}

DECLARE_THREAD_FN(asio_backend_dispatch)
{
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
          break;
        }
      } else {
        asio_event_t* ev = ep->udata;

        switch(ep->filter)
        {
          case EVFILT_READ:
            if(ep->data > 0)
              asio_event_send(ev, ASIO_READ);
            break;

          case EVFILT_WRITE:
            asio_event_send(ev, ASIO_WRITE);
            break;

          case EVFILT_TIMER:
            asio_event_send(ev, ASIO_TIMER);
            break;

          default: {}
        }
      }
    }

    handle_queue(b);
  }

  messageq_destroy(&b->q);
  POOL_FREE(asio_backend_t, b);
  return NULL;
}

void asio_event_subscribe(asio_event_t* ev)
{
  asio_backend_t* b = asio_get_backend();

  if(ev->noisy)
    asio_noisy_add();

  struct kevent event[3];
  int i = 0;

  // EV_CLEAR enforces edge triggered behaviour.
  if(ev->flags & ASIO_READ)
  {
    EV_SET(&event[i], ev->data, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, ev);
    i++;
  }

  if(ev->flags & ASIO_WRITE)
  {
    EV_SET(&event[i], ev->data, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, ev);
    i++;
  }

  if(ev->flags & ASIO_TIMER)
  {
    EV_SET(&event[i], (uintptr_t)ev, EVFILT_TIMER, EV_ADD | EV_ONESHOT,
      NOTE_NSECONDS, ev->data, ev);
    i++;
  }

  kevent(b->kq, event, i, NULL, 0, NULL);

  if(ev->data == STDIN_FILENO)
    retry_loop(b);
}

void asio_event_update(asio_event_t* ev, uintptr_t data)
{
  asio_backend_t* b = asio_get_backend();

  struct kevent event[1];
  int i = 0;

  if(ev->flags & ASIO_TIMER)
  {
    EV_SET(&event[i], (uintptr_t)ev, EVFILT_TIMER, EV_ADD | EV_ONESHOT,
      NOTE_NSECONDS, data, ev);
    i++;
  }

  kevent(b->kq, event, i, NULL, 0, NULL);
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

  struct kevent event[3];
  int i = 0;

  if(ev->flags & ASIO_READ)
  {
    EV_SET(&event[i], ev->data, EVFILT_READ, EV_DELETE, 0, 0, ev);
    i++;
  }

  if(ev->flags & ASIO_WRITE)
  {
    EV_SET(&event[i], ev->data, EVFILT_WRITE, EV_DELETE, 0, 0, ev);
    i++;
  }

  if(ev->flags & ASIO_TIMER)
  {
    EV_SET(&event[i], (uintptr_t)ev, EVFILT_TIMER, EV_DELETE, 0, 0, ev);
    i++;
  }

  kevent(b->kq, event, i, NULL, 0, NULL);

  asio_msg_t* msg = (asio_msg_t*)pony_alloc_msg(0, 0);
  msg->event = ev;
  msg->flags = 0;
  messageq_push(&b->q, (pony_msg_t*)msg);

  ev->flags = 0;
}

#endif
