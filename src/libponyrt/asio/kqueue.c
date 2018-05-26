#include "asio.h"
#include "event.h"
#ifdef ASIO_USE_KQUEUE

#include "../actor/messageq.h"
#include "../mem/pool.h"
#include "../sched/cpu.h"
#include "../sched/scheduler.h"
#include "ponyassert.h"
#include <sys/event.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>

#if defined(PLATFORM_IS_DRAGONFLY)
typedef u_short kevent_flag_t;
#else
typedef uint32_t kevent_flag_t;
#endif

struct asio_backend_t
{
  int kq;
  int wakeup[2];
  messageq_t q;
};

#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
static void empty_signal_handler(int sig)
{
  (void) sig;
}
#endif

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

#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
  // Make sure we ignore signals related to scheduler sleeping/waking
  // as the default for those signals is termination
  struct sigaction new_action;
  new_action.sa_handler = empty_signal_handler;
  sigemptyset (&new_action.sa_mask);

  // ask to restart interrupted syscalls to match `signal` behavior
  new_action.sa_flags = SA_RESTART;

  sigaction(PONY_SCHED_SLEEP_WAKE_SIGNAL, &new_action, NULL);
#endif

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

  while((msg = (asio_msg_t*)ponyint_thread_messageq_pop(
    &b->q
#ifdef USE_DYNAMIC_TRACE
    , SPECIAL_THREADID_KQUEUE
#endif
    )) != NULL)
  {
    pony_asio_event_send(msg->event, ASIO_DISPOSABLE, 0);
  }
}

static void retry_loop(asio_backend_t* b)
{
  char c = 0;
  write(b->wakeup[1], &c, 1);
}

PONY_API void pony_asio_event_resubscribe_read(asio_event_t* ev)
{
  if((ev == NULL) ||
    (ev->flags == ASIO_DISPOSABLE) ||
    (ev->flags == ASIO_DESTROYED))
  {
    pony_assert(0);
    return;
  }

  asio_backend_t* b = ponyint_asio_get_backend();
  pony_assert(b != NULL);

  struct kevent event[1];
  int i = 0;

  kevent_flag_t kqueue_flags = ev->flags & ASIO_ONESHOT ? EV_ONESHOT : EV_CLEAR;
  if((ev->flags & ASIO_READ) && !ev->readable)
  {
    EV_SET(&event[i], ev->fd, EVFILT_READ, EV_ADD | kqueue_flags, 0, 0, ev);
    i++;
  } else {
    return;
  }

  kevent(b->kq, event, i, NULL, 0, NULL);

  if(ev->fd == STDIN_FILENO)
    retry_loop(b);
}

PONY_API void pony_asio_event_resubscribe_write(asio_event_t* ev)
{
  if((ev == NULL) ||
    (ev->flags == ASIO_DISPOSABLE) ||
    (ev->flags == ASIO_DESTROYED))
  {
    pony_assert(0);
    return;
  }

  asio_backend_t* b = ponyint_asio_get_backend();
  pony_assert(b != NULL);

  struct kevent event[2];
  int i = 0;

  kevent_flag_t kqueue_flags = ev->flags & ASIO_ONESHOT ? EV_ONESHOT : EV_CLEAR;
  if((ev->flags & ASIO_WRITE) && !ev->writeable)
  {
    EV_SET(&event[i], ev->fd, EVFILT_WRITE, EV_ADD | kqueue_flags, 0, 0, ev);
    i++;
  } else {
    return;
  }

  kevent(b->kq, event, i, NULL, 0, NULL);

  if(ev->fd == STDIN_FILENO)
    retry_loop(b);
}

DECLARE_THREAD_FN(ponyint_asio_backend_dispatch)
{
  ponyint_cpu_affinity(ponyint_asio_get_cpu());
  pony_register_thread();
  asio_backend_t* b = arg;
  pony_assert(b != NULL);

#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
  // Make sure we block signals related to scheduler sleeping/waking
  // so they queue up to avoid race conditions
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, PONY_SCHED_SLEEP_WAKE_SIGNAL);
  pthread_sigmask(SIG_BLOCK, &set, NULL);
#endif

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
            if(ev->flags & ASIO_READ)
            {
              ev->readable = true;
              pony_asio_event_send(ev, ASIO_READ, 0);
            }
            break;

          case EVFILT_WRITE:
            if(ep->flags & EV_EOF)
            {
              if(ev->flags & (ASIO_READ | ASIO_WRITE))
              {
                ev->readable = true;
                ev->writeable = true;
                pony_asio_event_send(ev, ASIO_READ | ASIO_WRITE, 0);
              }
            } else {
              if(ev->flags & ASIO_WRITE)
              {
                ev->writeable = true;
                pony_asio_event_send(ev, ASIO_WRITE, 0);
              }
            }
            break;

          case EVFILT_TIMER:
            if(ev->flags & ASIO_TIMER)
            {
              pony_asio_event_send(ev, ASIO_TIMER, 0);
            }
            break;

          case EVFILT_SIGNAL:
            if(ev->flags & ASIO_SIGNAL)
            {
              pony_asio_event_send(ev, ASIO_SIGNAL, (uint32_t)ep->data);
            }
            break;

          default: {}
        }
      }
    }

    handle_queue(b);
  }

  ponyint_messageq_destroy(&b->q);
  POOL_FREE(asio_backend_t, b);
  pony_unregister_thread();
  return NULL;
}

PONY_API void pony_asio_event_subscribe(asio_event_t* ev)
{
  if((ev == NULL) ||
    (ev->flags == ASIO_DISPOSABLE) ||
    (ev->flags == ASIO_DESTROYED))
  {
    pony_assert(0);
    return;
  }

  asio_backend_t* b = ponyint_asio_get_backend();
  pony_assert(b != NULL);

  if(ev->noisy)
  {
    uint64_t old_count = ponyint_asio_noisy_add();
    // tell scheduler threads that asio has at least one noisy actor
    // if the old_count was 0
    if (old_count == 0)
      ponyint_sched_noisy_asio(SPECIAL_THREADID_KQUEUE);
  }

  struct kevent event[4];
  int i = 0;

  kevent_flag_t flags = ev->flags & ASIO_ONESHOT ? EV_ONESHOT : EV_CLEAR;
  if(ev->flags & ASIO_READ)
  {
    EV_SET(&event[i], ev->fd, EVFILT_READ, EV_ADD | flags, 0, 0, ev);
    i++;
  }

  if(ev->flags & ASIO_WRITE)
  {
    EV_SET(&event[i], ev->fd, EVFILT_WRITE, EV_ADD | flags, 0, 0, ev);
    i++;
  }

  if(ev->flags & ASIO_TIMER)
  {
#ifdef PLATFORM_IS_BSD
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
    // Make sure we ignore signals related to scheduler sleeping/waking
    // as the default for those signals is termination
    struct sigaction new_action;

    new_action.sa_handler = SIG_IGN;
#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
    if((int)ev->nsec == PONY_SCHED_SLEEP_WAKE_SIGNAL)
      new_action.sa_handler = empty_signal_handler;
#endif
    sigemptyset (&new_action.sa_mask);

    // ask to restart interrupted syscalls to match `signal` behavior
    new_action.sa_flags = SA_RESTART;

    sigaction((int)ev->nsec, &new_action, NULL);

    EV_SET(&event[i], ev->nsec, EVFILT_SIGNAL, EV_ADD | EV_CLEAR, 0, 0, ev);
    i++;
  }

  kevent(b->kq, event, i, NULL, 0, NULL);

  if(ev->fd == STDIN_FILENO)
    retry_loop(b);
}

PONY_API void pony_asio_event_setnsec(asio_event_t* ev, uint64_t nsec)
{
  if((ev == NULL) ||
    (ev->magic != ev) ||
    (ev->flags == ASIO_DISPOSABLE) ||
    (ev->flags == ASIO_DESTROYED))
  {
    pony_assert(0);
    return;
  }

  asio_backend_t* b = ponyint_asio_get_backend();
  pony_assert(b != NULL);

  struct kevent event[1];
  int i = 0;

  if(ev->flags & ASIO_TIMER)
  {
    ev->nsec = nsec;

#ifdef PLATFORM_IS_BSD
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

PONY_API void pony_asio_event_unsubscribe(asio_event_t* ev)
{
  if((ev == NULL) ||
    (ev->magic != ev) ||
    (ev->flags == ASIO_DISPOSABLE) ||
    (ev->flags == ASIO_DESTROYED))
  {
    pony_assert(0);
    return;
  }

  asio_backend_t* b = ponyint_asio_get_backend();
  pony_assert(b != NULL);

  if(ev->noisy)
  {
    uint64_t old_count = ponyint_asio_noisy_remove();
    // tell scheduler threads that asio has no noisy actors
    // if the old_count was 1
    if (old_count == 1)
    {
      ponyint_sched_unnoisy_asio(SPECIAL_THREADID_KQUEUE);

      // maybe wake up a scheduler thread if they've all fallen asleep
      ponyint_sched_maybe_wakeup_if_all_asleep(-1);
    }

    ev->noisy = false;
  }

  struct kevent event[4];
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
    // Make sure we ignore signals related to scheduler sleeping/waking
    // as the default for those signals is termination
    struct sigaction new_action;

    new_action.sa_handler = SIG_DFL;
#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
    if((int)ev->nsec == PONY_SCHED_SLEEP_WAKE_SIGNAL)
      new_action.sa_handler = empty_signal_handler;
#endif
    sigemptyset (&new_action.sa_mask);

    // ask to restart interrupted syscalls to match `signal` behavior
    new_action.sa_flags = SA_RESTART;

    sigaction((int)ev->nsec, &new_action, NULL);

    EV_SET(&event[i], ev->nsec, EVFILT_SIGNAL, EV_DELETE, 0, 0, ev);
    i++;
  }

  kevent(b->kq, event, i, NULL, 0, NULL);

  ev->flags = ASIO_DISPOSABLE;

  asio_msg_t* msg = (asio_msg_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(asio_msg_t)), 0);
  msg->event = ev;
  msg->flags = ASIO_DISPOSABLE;
  ponyint_thread_messageq_push(&b->q, (pony_msg_t*)msg, (pony_msg_t*)msg
#ifdef USE_DYNAMIC_TRACE
    , SPECIAL_THREADID_KQUEUE, SPECIAL_THREADID_KQUEUE
#endif
    );

  retry_loop(b);
}

#endif
