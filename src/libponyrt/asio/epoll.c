#define PONY_WANT_ATOMIC_DEFS

#include "asio.h"
#include "event.h"
#ifdef ASIO_USE_EPOLL

#include "../actor/messageq.h"
#include "../mem/pool.h"
#include "../sched/cpu.h"
#include "../sched/scheduler.h"
#include "../sched/systematic_testing.h"
#include "../tracing/tracing.h"
#include "ponyassert.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

#ifdef USE_VALGRIND
#include <valgrind/helgrind.h>
#endif

#define MAX_SIGNAL 128
#define MAX_SIGNAL_SUBSCRIBERS 16

#define ASIO_CANCEL_SIGNAL 10

typedef struct signal_subscribers_t {
  PONY_ATOMIC(int) eventfd;  // shared eventfd for this signal (-1 if none)
  PONY_ATOMIC(asio_event_t*) subscribers[MAX_SIGNAL_SUBSCRIBERS];
} signal_subscribers_t;

struct asio_backend_t
{
  int epfd;
  int wakeup;    /* eventfd to break epoll loop */
  struct epoll_event events[MAX_EVENTS];
  signal_subscribers_t sighandlers[MAX_SIGNAL];
  PONY_ATOMIC(bool) terminate;
  messageq_t q;
};

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

  eventfd_write(b->wakeup, 1);
}

static void signal_handler(int sig)
{
  if(sig >= MAX_SIGNAL)
    return;

  asio_backend_t* b = ponyint_asio_get_backend();
  pony_assert(b != NULL);

  // The eventfd is set (with release) before sigaction installs this handler,
  // so the happens-before ordering is established.
  int fd = atomic_load_explicit(&b->sighandlers[sig].eventfd,
    memory_order_acquire);
  if(fd < 0)
    return;

  eventfd_write(fd, 1);
}

#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
static void empty_signal_handler(int sig)
{
  (void) sig;
}
#endif

static void handle_queue(asio_backend_t* b)
{
  asio_msg_t* msg;

  while((msg = (asio_msg_t*)ponyint_thread_messageq_pop(&b->q
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
          int fd = atomic_load_explicit(&subs->eventfd, memory_order_acquire);
          if(fd >= 0)
          {
            // Last subscriber: restore default signal handling, clean up
            struct sigaction new_action;

#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
            if(sig == PONY_SCHED_SLEEP_WAKE_SIGNAL)
              new_action.sa_handler = empty_signal_handler;
            else
#endif
              new_action.sa_handler = SIG_DFL;

            sigemptyset(&new_action.sa_mask);
            new_action.sa_flags = SA_RESTART;
            sigaction(sig, &new_action, NULL);

            epoll_ctl(b->epfd, EPOLL_CTL_DEL, fd, NULL);
            close(fd);
            atomic_store_explicit(&subs->eventfd, -1, memory_order_release);
          }
        }

        ev->flags = ASIO_DISPOSABLE;
        pony_asio_event_send(ev, ASIO_DISPOSABLE, 0);
        break;
      }

      default: {}
    }
  }
}

asio_backend_t* ponyint_asio_backend_init()
{
  asio_backend_t* b = POOL_ALLOC(asio_backend_t);
  memset(b, 0, sizeof(asio_backend_t));
  ponyint_messageq_init(&b->q);

  // Initialize all signal eventfds to -1 (0 is a valid fd)
  for(int i = 0; i < MAX_SIGNAL; i++)
    atomic_store_explicit(&b->sighandlers[i].eventfd, -1,
      memory_order_relaxed);

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
  atomic_store_explicit(&b->terminate, true, memory_order_relaxed);
  eventfd_write(b->wakeup, 1);
}

// Single function for resubscribing to both reads and writes for an event
PONY_API void pony_asio_event_resubscribe(asio_event_t* ev)
{
  // needs to be a valid event that is one shot enabled
  if((ev == NULL) ||
    (ev->flags == ASIO_DISPOSABLE) ||
    (ev->flags == ASIO_DESTROYED) ||
    !(ev->flags & ASIO_ONESHOT))
  {
    pony_assert(0);
    return;
  }

  asio_backend_t* b = ponyint_asio_get_backend();
  pony_assert(b != NULL);

  struct epoll_event ep;
  ep.data.ptr = ev;
  ep.events = EPOLLONESHOT;
  bool something_to_resub = false;

  // if the event is supposed to be listening for write notifications
  // and it is currently not writeable
  if((ev->flags & ASIO_WRITE) && !ev->writeable)
  {
    something_to_resub = true;
    ep.events |= EPOLLOUT;
  }

  // if the event is supposed to be listening for read notifications
  // and it is currently not readable
  if((ev->flags & ASIO_READ) && !ev->readable)
  {
    something_to_resub = true;
    ep.events |= EPOLLRDHUP;
    ep.events |= EPOLLIN;
  }

  // only resubscribe if there is something to resubscribe to
  if (something_to_resub)
    epoll_ctl(b->epfd, EPOLL_CTL_MOD, ev->fd, &ep);
}

// Kept to maintain backwards compatibility so folks don't
// have to change their code to use `pony_asio_event_resubscribe`
// immediately
PONY_API void pony_asio_event_resubscribe_write(asio_event_t* ev)
{
  pony_asio_event_resubscribe(ev);
}

// Kept to maintain backwards compatibility so folks don't
// have to change their code to use `pony_asio_event_resubscribe`
// immediately
PONY_API void pony_asio_event_resubscribe_read(asio_event_t* ev)
{
  pony_asio_event_resubscribe(ev);
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

#if defined(USE_SYSTEMATIC_TESTING)
  // sleep thread until we're ready to start processing
  SYSTEMATIC_TESTING_WAIT_START(ponyint_asio_get_backend_tid(), ponyint_asio_get_backend_sleep_object());
#endif

  while(!atomic_load_explicit(&b->terminate, memory_order_acquire))
  {
    int wait_time = -1;
#if defined(USE_SYSTEMATIC_TESTING)
    wait_time = 10;
#endif

    int event_cnt = epoll_wait(b->epfd, b->events, MAX_EVENTS, wait_time);

    SYSTEMATIC_TESTING_YIELD();

    handle_queue(b);

    for(int i = 0; i < event_cnt; i++)
    {
      struct epoll_event* ep = &(b->events[i]);

      if(ep->data.ptr == b)
        continue;

      // Check if this is a signal event (ptr into sighandlers array)
      if(ep->data.ptr >= (void*)b->sighandlers &&
         ep->data.ptr < (void*)&b->sighandlers[MAX_SIGNAL])
      {
        signal_subscribers_t* subs = (signal_subscribers_t*)ep->data.ptr;
        if(ep->events & (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR))
        {
          int fd = atomic_load_explicit(&subs->eventfd, memory_order_acquire);
          if(fd >= 0)
          {
            uint64_t count;
            ssize_t rc = read(fd, &count, sizeof(uint64_t));
            (void)rc;
            for(size_t j = 0; j < MAX_SIGNAL_SUBSCRIBERS; j++)
            {
              asio_event_t* sub = atomic_load_explicit(
                &subs->subscribers[j], memory_order_acquire);
              if(sub != NULL)
                pony_asio_event_send(sub, ASIO_SIGNAL, (uint32_t)count);
            }
          }
        }
        continue;
      }

      asio_event_t* ev = ep->data.ptr;
      uint32_t flags = 0;
      uint32_t count = 0;

      if(ev->flags & ASIO_READ)
      {
        if(ep->events & (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR))
        {
          if(((ev->flags & ASIO_ONESHOT) && !ev->readable) || !(ev->flags & ASIO_ONESHOT))
          {
            ev->readable = true;
            flags |= ASIO_READ;
          }
        }
      }

      if(ev->flags & ASIO_WRITE)
      {
        if(ep->events & EPOLLOUT)
        {
          if(((ev->flags & ASIO_ONESHOT) && !ev->writeable) || !(ev->flags & ASIO_ONESHOT))
          {
            flags |= ASIO_WRITE;
            ev->writeable = true;
          }
        }
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

      if(flags != 0)
      {
        pony_asio_event_send(ev, flags, count);
      }
    }
  }

  close(b->epfd);
  close(b->wakeup);
  ponyint_messageq_destroy(&b->q, true);
  POOL_FREE(asio_backend_t, b);

  SYSTEMATIC_TESTING_STOP_THREAD();
  TRACING_THREAD_STOP();

  pony_unregister_thread();
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

    // Install the OS signal handler and add the subscriber synchronously
    // so they are active before subscribe returns. This prevents a race
    // where @raise fires before the handler or subscriber is installed.
    int expected = -1;
    if(atomic_compare_exchange_strong_explicit(&subs->eventfd, &expected, -2,
      memory_order_acq_rel, memory_order_acquire))
    {
      // We won the race — first subscriber. Set up the shared eventfd,
      // install sigaction, register with epoll.
      int fd = eventfd(0, EFD_NONBLOCK);

      struct sigaction new_action;
      new_action.sa_handler = signal_handler;
      sigemptyset(&new_action.sa_mask);
      new_action.sa_flags = SA_RESTART;

      struct epoll_event ep;
      ep.data.ptr = subs;
      ep.events = EPOLLIN | EPOLLET;
      epoll_ctl(b->epfd, EPOLL_CTL_ADD, fd, &ep);

      // Store the fd with release ordering BEFORE installing sigaction.
      // sigaction provides additional ordering, but the release store
      // ensures signal_handler sees the fd.
      atomic_store_explicit(&subs->eventfd, fd, memory_order_release);

      sigaction(sig, &new_action, NULL);
    }
    else
    {
      // Another thread is setting up this signal (-2 sentinel). Spin until
      // setup completes so that signal_handler can see a valid eventfd.
      // This loop is very brief — it only waits for eventfd creation,
      // epoll registration, and one atomic store.
      while(atomic_load_explicit(&subs->eventfd, memory_order_acquire) < 0)
        ;
    }

    // Add subscriber synchronously using CAS on array slots. This ensures
    // the subscriber is visible before the calling actor's next behavior
    // (e.g., raise) executes, preventing a race where the signal fires
    // before the subscriber is in the list.
    bool added = false;
    for(size_t i = 0; i < MAX_SIGNAL_SUBSCRIBERS; i++)
    {
      asio_event_t* expected = NULL;
      if(atomic_compare_exchange_strong_explicit(&subs->subscribers[i],
        &expected, ev, memory_order_release, memory_order_relaxed))
      {
        added = true;
        break;
      }
    }
    pony_assert(added);
    (void)added;
    return;
  }

  struct epoll_event ep;
  ep.data.ptr = ev;
  ep.events = EPOLLRDHUP;

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

  if(ev->flags & ASIO_ONESHOT)
  {
    ep.events |= EPOLLONESHOT;
  } else {
    ep.events |= EPOLLET;
  }

  epoll_ctl(b->epfd, EPOLL_CTL_ADD, ev->fd, &ep);
}

PONY_API void pony_asio_event_setnsec(asio_event_t* ev, uint64_t nsec)
{
  if((ev == NULL) ||
    (ev->flags == ASIO_DISPOSABLE) ||
    (ev->flags == ASIO_DESTROYED))
  {
    pony_assert(0);
    return;
  }

  if(ev->flags & ASIO_TIMER)
  {
    ev->nsec = nsec;
    timer_set_nsec(ev->fd, nsec);
  }
}

PONY_API void pony_asio_event_unsubscribe(asio_event_t* ev)
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

  if(ev->flags & ASIO_SIGNAL)
  {
    // Signal unregistration is serialized through the ASIO thread.
    // The shared eventfd and epoll cleanup are handled in ASIO_CANCEL_SIGNAL.
    send_request(ev, ASIO_CANCEL_SIGNAL);
    return;
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

  ev->flags = ASIO_DISPOSABLE;
  send_request(ev, ASIO_DISPOSABLE);
}

#endif
