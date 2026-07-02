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

#define MAX_SIGNAL 128
// Coupled: the 16-subscriber cap is documented API behavior — see
// .known-couplings/signal-subscriber-cap.md before changing it.
#define MAX_SIGNAL_SUBSCRIBERS 16

// ASIO_ERROR arg codes for signal registration failures. These values are
// a contract with packages/signals/signal_handler.pony's reason mapping —
// see the arg contract note in
// .known-couplings/asio-signal-registration-protocol.md.
#define SIGNAL_ERR_SUBSCRIBER_LIMIT 1
#define SIGNAL_ERR_REFUSED 2

#define ASIO_CANCEL_SIGNAL 10

// Signal registration protocol (same shape in kqueue.c, sock_notify.c, and
// here — see .known-couplings/asio-signal-registration-protocol.md).
// `registered` is a tri-state: 0 = no OS registration,
// -1 = a thread is mid-install or the ASIO thread is mid-teardown,
// 1 = registered. Subscribe (any scheduler thread) loops: wait for 1
// (installing via CAS 0 -> -1 if first), CAS into a free subscriber slot,
// then RE-VERIFY the state is still 1 — retrying from the top if a
// concurrent last-subscriber cancel tore the registration down between the
// wait and the slot insert. Cancel (ASIO thread only) removes the slot, and
// only tears down after claiming the registration (CAS 1 -> -1) and
// re-scanning the slots for a racing insert. The slot-insert CAS, the
// re-verify load, the claim CAS, and the post-claim re-scan loads are all
// seq_cst: the subscriber does store(slot) -> load(state) while the canceler
// does RMW(state) -> load(slots) — a store-buffer shape where
// acquire/release alone lets both sides miss each other (masked on x86, real
// on weaker architectures). The publish of 1 happens only after the
// sigaction install, so "subscribe returned" implies the handler is active.
// No systematic-testing yield or parking call may ever be introduced inside
// the claimed (-1) windows: the subscribe spin has no yield point and is
// deadlock-free under systematic testing only because -1 is never observable
// there. The delivery channel is kept SEPARATE from the protocol state: `eventfd`
// holds the shared eventfd and stays valid through a claim that aborts, so
// a claim can never drop a delivery for a still-registered subscriber; it
// is invalidated only on committed teardown. Signal-context delivery keeps
// its pre-existing benign race (the handler can race a committed teardown's
// close of the eventfd, which existed before this design too).
typedef struct signal_subscribers_t {
  PONY_ATOMIC(int) registered;  // 0=no, -1=in-progress, 1=yes
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

  // The eventfd is stored before sigaction installs this handler and stays
  // valid until committed teardown (including through a claim that aborts),
  // so a raise() after subscribe returned always sees a live fd. An
  // externally delivered signal racing an install or a committed teardown
  // can read -1 and be dropped — no worse than it arriving before
  // registration or after unsubscription. Pre-existing and accepted: a
  // handler preempted between loading a live fd and writing it can race a
  // committed teardown's close, and in the worst case writes to an
  // unrelated reused descriptor (the old design had the same race via
  // ev->fd).
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
        // A second queued cancel for an already-disposed event (raw-FFI
        // double-unsubscribe) must not produce a second DISPOSABLE
        // notification — the owner would destroy the event twice. This
        // thread is the only writer of ASIO_DISPOSABLE for signal events,
        // so the check is reliable.
        if(ev->flags == ASIO_DISPOSABLE)
          break;

        if(ev->nsec >= MAX_SIGNAL)
        {
          ev->flags = ASIO_DISPOSABLE;
          pony_asio_event_send(ev, ASIO_DISPOSABLE, 0);
          break;
        }

        int sig = (int)ev->nsec;
        signal_subscribers_t* subs = &b->sighandlers[sig];

        // Remove ev's slot from the subscriber array.
        for(size_t i = 0; i < MAX_SIGNAL_SUBSCRIBERS; i++)
        {
          if(atomic_load_explicit(&subs->subscribers[i],
            memory_order_acquire) == ev)
          {
            // Clear every matching slot, not just the first: a raw-FFI
            // caller that subscribed the same event twice occupies two
            // slots, and leaving one behind after the event is destroyed
            // turns the next fan-out into a use-after-free.
            atomic_store_explicit(&subs->subscribers[i], NULL,
              memory_order_release);
          }
        }

        // Check whether any subscribers remain.
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
          // Claim the registration before tearing down (see the protocol
          // comment at signal_subscribers_t). The CAS fails if nothing is
          // registered or an install is in progress — then teardown is not
          // ours to do. The eventfd stays live through the claim, so
          // deliveries for a racing subscriber are never dropped; it is
          // invalidated only once the teardown commits.
          int expected = 1;
          if(atomic_compare_exchange_strong_explicit(&subs->registered,
            &expected, -1, memory_order_seq_cst, memory_order_seq_cst))
          {
            // Re-scan: a subscriber may have slot-inserted between the scan
            // above and the claim. seq_cst per the protocol comment.
            bool still_empty = true;
            for(size_t i = 0; i < MAX_SIGNAL_SUBSCRIBERS; i++)
            {
              if(atomic_load_explicit(&subs->subscribers[i],
                memory_order_seq_cst) != NULL)
              {
                still_empty = false;
                break;
              }
            }

            if(still_empty)
            {
              // Last subscriber is gone: restore default signal handling,
              // clean up.
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

              int fd = atomic_load_explicit(&subs->eventfd,
                memory_order_acquire);
              epoll_ctl(b->epfd, EPOLL_CTL_DEL, fd, NULL);
              atomic_store_explicit(&subs->eventfd, -1,
                memory_order_seq_cst);
              close(fd);
              atomic_store_explicit(&subs->registered, 0,
                memory_order_seq_cst);
            } else {
              // A racing subscribe slipped in — abort the teardown.
              atomic_store_explicit(&subs->registered, 1,
                memory_order_seq_cst);
            }
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

  if(b->epfd == -1 || b->wakeup == -1)
  {
    if(b->epfd != -1)
      close(b->epfd);
    if(b->wakeup != -1)
      close(b->wakeup);
    ponyint_messageq_destroy(&b->q, true);
    POOL_FREE(asio_backend_t, b);
    return NULL;
  }

  struct epoll_event ep;
  ep.data.ptr = b;
  ep.events = EPOLLIN | EPOLLRDHUP | EPOLLET;

  if(epoll_ctl(b->epfd, EPOLL_CTL_ADD, b->wakeup, &ep) == -1)
  {
    close(b->epfd);
    close(b->wakeup);
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
  {
    if(epoll_ctl(b->epfd, EPOLL_CTL_MOD, ev->fd, &ep) == -1)
      pony_asio_event_send(ev, ASIO_ERROR, 0);
  }
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
    // Under systematic testing execution is serialized to one thread at a time,
    // so any real wait here stalls the whole program while it is our turn. Poll
    // instead: report whatever is already ready and hand our turn straight back.
    // (A normal build leaves wait_time at -1 and blocks until the kernel has an
    // event.)
    wait_time = 0;
#endif

    int event_cnt = epoll_wait(b->epfd, b->events, MAX_EVENTS, wait_time);

    SYSTEMATIC_TESTING_YIELD();

    for(int i = 0; i < event_cnt; i++)
    {
      struct epoll_event* ep = &(b->events[i]);

      if(ep->data.ptr == b)
        continue;

      // Check if this is a signal event (ptr into sighandlers array).
      // Compared as integers: a relational compare of pointers into
      // different objects is formally UB, and this check decides which
      // struct type the pointer is cast to.
      if((uintptr_t)ep->data.ptr >= (uintptr_t)b->sighandlers &&
         (uintptr_t)ep->data.ptr < (uintptr_t)&b->sighandlers[MAX_SIGNAL])
      {
        signal_subscribers_t* subs = (signal_subscribers_t*)ep->data.ptr;
        if(ep->events & (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR))
        {
          int fd = atomic_load_explicit(&subs->eventfd, memory_order_acquire);
          if(fd >= 0)
          {
            uint64_t count;

            // Only fan out on a successful read: a spurious edge-triggered
            // wake would otherwise forward an uninitialized count.
            if(read(fd, &count, sizeof(uint64_t)) == sizeof(uint64_t))
            {
              for(size_t j = 0; j < MAX_SIGNAL_SUBSCRIBERS; j++)
              {
                asio_event_t* sub = atomic_load_explicit(
                  &subs->subscribers[j], memory_order_acquire);
                if(sub != NULL)
                  pony_asio_event_send(sub, ASIO_SIGNAL, (uint32_t)count);
              }
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
          // Send read notification to an actor if either
          // * the event is not a one shot event
          // * the event is a one shot event and we haven't already sent a notification
          // if the event is a one shot event and we have already sent a notification
          // don't send another one until we are asked for it again (i.e. the actor
          // gets a 0 byte read and sets `readable` to false and resubscribes to reads
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
          // Send write notification to an actor if either
          // * the event is not a one shot event
          // * the event is a one shot event and we haven't already sent a notification
          // if the event is a one shot event and we have already sent a notification
          // don't send another one until we are asked for it again (i.e. the actor
          // gets partial write and sets `writeable` to false and resubscribes to writes
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

      // if we had a valid event of some type that needs to be sent
      // to an actor
      if(flags != 0)
      {
        // send the event to the actor
        pony_asio_event_send(ev, flags, count);
      }
    }

    handle_queue(b);
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

static bool timer_set_nsec(int fd, uint64_t nsec)
{
  struct itimerspec ts;

  ts.it_interval.tv_sec = 0;
  ts.it_interval.tv_nsec = 0;
  ts.it_value.tv_sec = (time_t)(nsec / 1000000000);
  ts.it_value.tv_nsec = (long)(nsec - (ts.it_value.tv_sec * 1000000000));

  return timerfd_settime(fd, 0, &ts, NULL) == 0;
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
    if(ev->nsec >= MAX_SIGNAL)
    {
      // Out of range (only reachable through raw FFI — ValidSignal caps
      // well below MAX_SIGNAL). Report it so the handler auto-disposes
      // instead of stranding, per the documented contract.
      pony_asio_event_send(ev, ASIO_ERROR, SIGNAL_ERR_REFUSED);
      return;
    }

    int sig = (int)ev->nsec;
    signal_subscribers_t* subs = &b->sighandlers[sig];

    // Register synchronously so both the OS handler and the subscriber slot
    // are active before subscribe returns — otherwise a raise executed right
    // after subscribing could take the default disposition (process death) or
    // miss this subscriber. See the protocol comment at signal_subscribers_t.
    while(true)
    {
      int state = atomic_load_explicit(&subs->registered,
        memory_order_acquire);

      if(state == 0)
      {
        int expected = 0;
        if(atomic_compare_exchange_strong_explicit(&subs->registered,
          &expected, -1, memory_order_seq_cst, memory_order_seq_cst))
        {
          // First subscriber: create the shared eventfd and register it with
          // epoll (checked — a failure is reported instead of leaving a dead
          // registration), install sigaction, then publish. The publish comes
          // last so "subscribe returned" implies the handler is installed.
          int fd = eventfd(0, EFD_NONBLOCK);

          struct epoll_event sep;
          sep.data.ptr = subs;
          sep.events = EPOLLIN | EPOLLET;

          if((fd == -1) || (epoll_ctl(b->epfd, EPOLL_CTL_ADD, fd, &sep) == -1))
          {
            // The fd was never published, so cleanup here is purely
            // local; sigaction hasn't run yet, so the disposition needs no
            // rollback.
            if(fd != -1)
              close(fd);
            atomic_store_explicit(&subs->registered, 0,
              memory_order_seq_cst);
            pony_asio_event_send(ev, ASIO_ERROR, SIGNAL_ERR_REFUSED);
            return;
          }

          struct sigaction new_action;
          new_action.sa_handler = signal_handler;
          sigemptyset(&new_action.sa_mask);

          // ask to restart interrupted syscalls to match `signal` behavior
          new_action.sa_flags = SA_RESTART;

          // The fd is stored before sigaction so the handler always sees it.
          atomic_store_explicit(&subs->eventfd, fd, memory_order_seq_cst);

          if(sigaction(sig, &new_action, NULL) == -1)
          {
            // The OS refused the handler (e.g. glibc reserves RT signals 32
            // and 33 for its threading internals — musl 32 through 34 — and
            // rejects them with EINVAL). Unwind the eventfd registration and
            // report, so the failure surfaces as an auto-disposed handler
            // instead of a registration that never delivers.
            epoll_ctl(b->epfd, EPOLL_CTL_DEL, fd, NULL);
            atomic_store_explicit(&subs->eventfd, -1, memory_order_seq_cst);
            close(fd);
            atomic_store_explicit(&subs->registered, 0,
              memory_order_seq_cst);
            pony_asio_event_send(ev, ASIO_ERROR, SIGNAL_ERR_REFUSED);
            return;
          }

          atomic_store_explicit(&subs->registered, 1, memory_order_seq_cst);
          state = 1;
        }
      }

      if(state != 1)
      {
        // Another thread is mid-install or the ASIO thread is mid-teardown.
        // This wait is very brief — an eventfd, an epoll_ctl, a sigaction,
        // and one atomic store.
        ponyint_cpu_relax();
        continue;
      }

      // Insert into a free subscriber slot. seq_cst per the protocol comment.
      size_t slot = MAX_SIGNAL_SUBSCRIBERS;
      for(size_t i = 0; i < MAX_SIGNAL_SUBSCRIBERS; i++)
      {
        asio_event_t* expected = NULL;
        if(atomic_compare_exchange_strong_explicit(&subs->subscribers[i],
          &expected, ev, memory_order_seq_cst, memory_order_seq_cst))
        {
          slot = i;
          break;
        }
      }

      if(slot == MAX_SIGNAL_SUBSCRIBERS)
      {
        // All subscriber slots are taken. Report it; the stdlib disposes the
        // handler on an errored event.
        pony_asio_event_send(ev, ASIO_ERROR, SIGNAL_ERR_SUBSCRIBER_LIMIT);
        return;
      }

      // Re-verify the registration is still live; a concurrent last-cancel
      // teardown may have run between the state check and the slot insert.
      // seq_cst per the protocol comment.
      if(atomic_load_explicit(&subs->registered, memory_order_seq_cst) == 1)
        return;

      // Teardown raced us: undo the slot insert and retry from the top.
      atomic_store_explicit(&subs->subscribers[slot], NULL,
        memory_order_release);
    }
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
    if(ev->fd == -1)
    {
      pony_asio_event_send(ev, ASIO_ERROR, 0);
      return;
    }

    if(!timer_set_nsec(ev->fd, ev->nsec))
    {
      close(ev->fd);
      ev->fd = -1;
      pony_asio_event_send(ev, ASIO_ERROR, 0);
      return;
    }

    ep.events |= EPOLLIN;
  }

  if(ev->flags & ASIO_ONESHOT)
  {
    ep.events |= EPOLLONESHOT;
  } else {
    // Only use edge triggering if one shot isn't enabled.
    // This is because of how the runtime gets notifications
    // from epoll in this ASIO thread and then notifies the
    // appropriate actor to read/write as necessary.
    // specifically, it seems there's an edge case/race condition
    // with edge triggering where if there is already data waiting
    // on the socket, then epoll might not be triggering immediately
    // when an edge triggered epoll request is made.

    ep.events |= EPOLLET;
  }

  if(epoll_ctl(b->epfd, EPOLL_CTL_ADD, ev->fd, &ep) == -1)
    pony_asio_event_send(ev, ASIO_ERROR, 0);
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
    if(!timer_set_nsec(ev->fd, nsec))
      pony_asio_event_send(ev, ASIO_ERROR, 0);
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

  // Don't send ASIO_ERROR on delete failure — the actor is tearing down,
  // and ENOENT/EBADF is expected (FD already closed or never registered).
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
