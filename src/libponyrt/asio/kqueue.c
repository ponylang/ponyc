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

struct asio_backend_t
{
  int kq;
  int wakeup[2];
  messageq_t q;
};

// Returns true if any entry in EV_RECEIPT results represents a real error.
// With EV_RECEIPT, every result has EV_ERROR in flags; the distinguishing
// factor is whether data is zero (success) or non-zero (errno).
// Ignores EPIPE on macOS — the kernel delivers events despite the error.
static bool kevent_receipt_has_error(struct kevent* results, int count)
{
  for(int i = 0; i < count; i++)
  {
    if(results[i].data != 0)
    {
#ifdef PLATFORM_IS_MACOSX
      if(results[i].data == EPIPE)
        continue;
#endif
      return true;
    }
  }

  return false;
}

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
    ponyint_messageq_destroy(&b->q, true);
    POOL_FREE(asio_backend_t, b);
    return NULL;
  }

  if(pipe(b->wakeup) == -1)
  {
    close(b->kq);
    ponyint_messageq_destroy(&b->q, true);
    POOL_FREE(asio_backend_t, b);
    return NULL;
  }

  struct kevent new_event;
  EV_SET(&new_event, b->wakeup[0], EVFILT_READ, EV_ADD, 0, 0, NULL);

  struct timespec t = {0, 0};

  if(kevent(b->kq, &new_event, 1, NULL, 0, &t) == -1)
  {
    close(b->kq);
    close(b->wakeup[0]);
    close(b->wakeup[1]);
    ponyint_messageq_destroy(&b->q, true);
    POOL_FREE(asio_backend_t, b);
    return NULL;
  }

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
    EV_SET(&event[i], ev->fd, EVFILT_READ,
      EV_ADD | EV_RECEIPT | kqueue_flags, 0, 0, ev);
    i++;
  } else {
    return;
  }

  struct kevent results[1];
  int rc = kevent(b->kq, event, i, results, i, NULL);

  if((rc == -1) || kevent_receipt_has_error(results, i))
    pony_asio_event_send(ev, ASIO_ERROR, 0);

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
    EV_SET(&event[i], ev->fd, EVFILT_WRITE,
      EV_ADD | EV_RECEIPT | kqueue_flags, 0, 0, ev);
    i++;
  } else {
    return;
  }

  struct kevent results[1];
  int rc = kevent(b->kq, event, i, results, i, NULL);

  if((rc == -1) || kevent_receipt_has_error(results, i))
    pony_asio_event_send(ev, ASIO_ERROR, 0);

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

  struct kevent event[4];
  int i = 0;

  kevent_flag_t flags = ev->flags & ASIO_ONESHOT ? EV_ONESHOT : EV_CLEAR;
  if(ev->flags & ASIO_READ)
  {
    EV_SET(&event[i], ev->fd, EVFILT_READ,
      EV_ADD | EV_RECEIPT | flags, 0, 0, ev);
    i++;
  }

  if(ev->flags & ASIO_WRITE)
  {
    EV_SET(&event[i], ev->fd, EVFILT_WRITE,
      EV_ADD | EV_RECEIPT | flags, 0, 0, ev);
    i++;
  }

  if(ev->flags & ASIO_TIMER)
  {
#ifdef PLATFORM_IS_BSD
    EV_SET(&event[i], (uintptr_t)ev, EVFILT_TIMER,
      EV_ADD | EV_RECEIPT | EV_ONESHOT, 0, ev->nsec / 1000000, ev);
#else
    EV_SET(&event[i], (uintptr_t)ev, EVFILT_TIMER,
      EV_ADD | EV_RECEIPT | EV_ONESHOT, NOTE_NSECONDS, ev->nsec, ev);
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

    EV_SET(&event[i], ev->nsec, EVFILT_SIGNAL,
      EV_ADD | EV_RECEIPT | EV_CLEAR, 0, 0, ev);
    i++;
  }

  struct kevent results[4];
  int rc = kevent(b->kq, event, i, results, i, NULL);

  if((rc == -1) || kevent_receipt_has_error(results, i))
    pony_asio_event_send(ev, ASIO_ERROR, 0);

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
    EV_SET(&event[i], (uintptr_t)ev, EVFILT_TIMER,
      EV_ADD | EV_RECEIPT | EV_ONESHOT, 0, ev->nsec / 1000000, ev);
#else
    EV_SET(&event[i], (uintptr_t)ev, EVFILT_TIMER,
      EV_ADD | EV_RECEIPT | EV_ONESHOT, NOTE_NSECONDS, ev->nsec, ev);
#endif
    i++;
  }

  struct kevent results[1];
  int rc = kevent(b->kq, event, i, results, i, NULL);

  if((rc == -1) || kevent_receipt_has_error(results, i))
    pony_asio_event_send(ev, ASIO_ERROR, 0);
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

  struct kevent event[4];
  int i = 0;

  if(ev->flags & ASIO_READ)
  {
    EV_SET(&event[i], ev->fd, EVFILT_READ, EV_DELETE | EV_RECEIPT, 0, 0, ev);
    i++;
  }

  if(ev->flags & ASIO_WRITE)
  {
    EV_SET(&event[i], ev->fd, EVFILT_WRITE, EV_DELETE | EV_RECEIPT, 0, 0, ev);
    i++;
  }

  if(ev->flags & ASIO_TIMER)
  {
    EV_SET(&event[i], (uintptr_t)ev, EVFILT_TIMER,
      EV_DELETE | EV_RECEIPT, 0, 0, ev);
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

    EV_SET(&event[i], ev->nsec, EVFILT_SIGNAL,
      EV_DELETE | EV_RECEIPT, 0, 0, ev);
    i++;
  }

  // EV_RECEIPT ensures all EV_DELETE entries are processed regardless of
  // individual failures. We don't send ASIO_ERROR here — the actor is
  // tearing down, and ENOENT/EBADF on EV_DELETE is expected and benign.
  struct kevent results[4];
  kevent(b->kq, event, i, results, i, NULL);

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
