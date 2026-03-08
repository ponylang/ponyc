#define PONY_WANT_ATOMIC_DEFS

#include "asio.h"
#include "event.h"
#ifdef ASIO_USE_KQUEUE

#include "../actor/messageq.h"
#include "../mem/pool.h"
#include "../sched/cpu.h"
#include "../sched/scheduler.h"
#include "../sched/systematic_testing.h"
#include "../tracing/tracing.h"
#include "ponyassert.h"
#include <sys/event.h>
#include <sys/time.h>
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

#define MAX_SIGNAL 128
#define MAX_SIGNAL_SUBSCRIBERS 16

#define ASIO_CANCEL_SIGNAL 10

typedef struct signal_subscribers_t {
  PONY_ATOMIC(int) registered;  // 0=no, -1=in-progress, 1=yes
  PONY_ATOMIC(asio_event_t*) subscribers[MAX_SIGNAL_SUBSCRIBERS];
} signal_subscribers_t;

struct asio_backend_t
{
  int kq;
  int wakeup[2];
  signal_subscribers_t sighandlers[MAX_SIGNAL];
  messageq_t q;
};

#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
static void empty_signal_handler(int sig)
{
  (void) sig;
}
#endif

static void retry_loop(asio_backend_t* b)
{
  char c = 0;
  write(b->wakeup[1], &c, 1);
}

static void send_request(asio_event_t* ev, int req)
{
  asio_backend_t* b = ponyint_asio_get_backend();
  pony_assert(b != NULL);

  asio_msg_t* msg = (asio_msg_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(asio_msg_t)), 0);
  msg->event = ev;
  msg->flags = req;
  ponyint_thread_messageq_push(&b->q, (pony_msg_t*)msg, (pony_msg_t*)msg
#ifdef USE_DYNAMIC_TRACE
    , pony_scheduler_index(), pony_scheduler_index()
#endif
    );

  retry_loop(b);
}

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
    , pony_scheduler_index()
#endif
    )) != NULL)
  {
    asio_event_t* ev = msg->event;

    switch(msg->flags)
    {
      case ASIO_DISPOSABLE:
        pony_asio_event_send(ev, ASIO_DISPOSABLE, 0);
        break;

      case ASIO_CANCEL_SIGNAL:
      {
        int sig = (int)ev->nsec;

        if(sig >= MAX_SIGNAL)
        {
          ev->flags = ASIO_DISPOSABLE;
          pony_asio_event_send(ev, ASIO_DISPOSABLE, 0);
          break;
        }

        signal_subscribers_t* subs = &b->sighandlers[sig];

        // Remove ev from subscriber array (set slot to NULL)
        for(size_t i = 0; i < MAX_SIGNAL_SUBSCRIBERS; i++)
        {
          if(atomic_load_explicit(&subs->subscribers[i],
            memory_order_acquire) == ev)
          {
            atomic_store_explicit(&subs->subscribers[i], NULL,
              memory_order_release);
            break;
          }
        }

        // Check if all subscribers are gone
        bool has_subscribers = false;
        for(size_t i = 0; i < MAX_SIGNAL_SUBSCRIBERS; i++)
        {
          if(atomic_load_explicit(&subs->subscribers[i],
            memory_order_acquire) != NULL)
          {
            has_subscribers = true;
            break;
          }
        }

        if(!has_subscribers)
        {
          // Last subscriber: restore default signal handling, remove kevent
          struct sigaction new_action;

          new_action.sa_handler = SIG_DFL;
#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
          if(sig == PONY_SCHED_SLEEP_WAKE_SIGNAL)
            new_action.sa_handler = empty_signal_handler;
#endif
          sigemptyset(&new_action.sa_mask);
          new_action.sa_flags = SA_RESTART;
          sigaction(sig, &new_action, NULL);

          struct kevent event;
          EV_SET(&event, sig, EVFILT_SIGNAL, EV_DELETE, 0, 0, subs);
          struct timespec t = {0, 0};
          kevent(b->kq, &event, 1, NULL, 0, &t);

          atomic_store_explicit(&subs->registered, 0, memory_order_release);
        }

        ev->flags = ASIO_DISPOSABLE;
        pony_asio_event_send(ev, ASIO_DISPOSABLE, 0);
        break;
      }

      default: {}
    }
  }
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
  ponyint_register_asio_thread();
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

#if defined(USE_SYSTEMATIC_TESTING)
  // sleep thread until we're ready to start processing
  SYSTEMATIC_TESTING_WAIT_START(ponyint_asio_get_backend_tid(), ponyint_asio_get_backend_sleep_object());
#endif

  while(b->kq != -1)
  {
    struct timespec* timeout = NULL;
#if defined(USE_SYSTEMATIC_TESTING)
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 10000000;
    timeout = &ts;
#endif

    int count = kevent(b->kq, NULL, 0, fired, MAX_EVENTS, timeout);

    SYSTEMATIC_TESTING_YIELD();

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
        if(ep->flags & EV_ERROR)
          continue;

        // Handle signal events before casting udata to asio_event_t*,
        // because for signals udata points to a signal_subscribers_t*.
        if(ep->filter == EVFILT_SIGNAL)
        {
          signal_subscribers_t* subs = (signal_subscribers_t*)ep->udata;
          for(size_t j = 0; j < MAX_SIGNAL_SUBSCRIBERS; j++)
          {
            asio_event_t* sub = atomic_load_explicit(
              &subs->subscribers[j], memory_order_acquire);
            if(sub != NULL)
              pony_asio_event_send(sub, ASIO_SIGNAL, (uint32_t)ep->data);
          }
          continue;
        }

        asio_event_t* ev = ep->udata;

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

          default: {}
        }
      }
    }

    handle_queue(b);
  }

  ponyint_messageq_destroy(&b->q, true);
  POOL_FREE(asio_backend_t, b);

  SYSTEMATIC_TESTING_STOP_THREAD();
  TRACING_THREAD_STOP();

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
      ponyint_sched_noisy_asio(pony_scheduler_index());
  }

  if(ev->flags & ASIO_SIGNAL)
  {
    int sig = (int)ev->nsec;

    if(sig >= MAX_SIGNAL)
      return;

    signal_subscribers_t* subs = &b->sighandlers[sig];

    // Install kevent and sigaction synchronously on first subscriber.
    // kqueue uses SIG_IGN — EVFILT_SIGNAL detects signals regardless
    // of disposition, and SIG_IGN prevents default termination.
    int expected = 0;
    if(atomic_compare_exchange_strong_explicit(&subs->registered, &expected,
      -1, memory_order_acq_rel, memory_order_acquire))
    {
      struct sigaction new_action;
      new_action.sa_handler = SIG_IGN;
#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
      if(sig == PONY_SCHED_SLEEP_WAKE_SIGNAL)
        new_action.sa_handler = empty_signal_handler;
#endif
      sigemptyset(&new_action.sa_mask);
      new_action.sa_flags = SA_RESTART;
      sigaction(sig, &new_action, NULL);

      struct kevent event;
      EV_SET(&event, sig, EVFILT_SIGNAL, EV_ADD | EV_CLEAR, 0, 0, subs);
      struct timespec t = {0, 0};
      kevent(b->kq, &event, 1, NULL, 0, &t);

      atomic_store_explicit(&subs->registered, 1, memory_order_release);
    }
    else
    {
      // Another thread is setting up this signal. Spin until setup completes.
      while(atomic_load_explicit(&subs->registered, memory_order_acquire) < 0)
        ;
    }

    // Add subscriber synchronously using CAS on array slots
    bool added = false;
    for(size_t i = 0; i < MAX_SIGNAL_SUBSCRIBERS; i++)
    {
      asio_event_t* exp = NULL;
      if(atomic_compare_exchange_strong_explicit(&subs->subscribers[i],
        &exp, ev, memory_order_release, memory_order_relaxed))
      {
        added = true;
        break;
      }
    }
    pony_assert(added);
    (void)added;
    return;
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

  if(ev->flags & ASIO_SIGNAL)
  {
    // Signal unregistration is serialized through the ASIO thread.
    // The kevent cleanup and sigaction restore are handled in
    // ASIO_CANCEL_SIGNAL.
    send_request(ev, ASIO_CANCEL_SIGNAL);
    return;
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

  kevent(b->kq, event, i, NULL, 0, NULL);

  ev->flags = ASIO_DISPOSABLE;

  asio_msg_t* msg = (asio_msg_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(asio_msg_t)), 0);
  msg->event = ev;
  msg->flags = ASIO_DISPOSABLE;
  ponyint_thread_messageq_push(&b->q, (pony_msg_t*)msg, (pony_msg_t*)msg
#ifdef USE_DYNAMIC_TRACE
    , pony_scheduler_index(), pony_scheduler_index()
#endif
    );

  retry_loop(b);
}

#endif
