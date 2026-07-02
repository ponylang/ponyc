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

// Signal registration protocol (same shape in epoll.c, sock_notify.c, and
// here — see .known-couplings/asio-signal-registration-protocol.md).
// `registered` is a tri-state: 0 = no OS registration, -1 = a thread is
// mid-install or the ASIO thread is mid-teardown, 1 = registered.
// Subscribe (any scheduler thread) loops: wait for 1 (installing via
// CAS 0 -> -1 if first), CAS into a free subscriber slot, then RE-VERIFY the
// state is still 1 — retrying from the top if a concurrent last-subscriber
// cancel tore the registration down between the wait and the slot insert.
// Cancel (ASIO thread only) removes the slot, and only tears down after
// claiming the registration (CAS 1 -> -1) and re-scanning the slots for a
// racing insert. The slot-insert CAS, the re-verify load, the claim CAS, and
// the post-claim re-scan loads are all seq_cst: the subscriber does
// store(slot) -> load(state) while the canceler does RMW(state) -> load(slots)
// — a store-buffer shape where acquire/release alone lets both sides miss
// each other (masked on x86, real on weaker architectures). The publish of 1
// happens only after the sigaction install and the checked EVFILT_SIGNAL
// registration, so "subscribe returned" implies the signal is being detected.
// No systematic-testing yield or parking call may ever be introduced inside
// the claimed (-1) windows: the subscribe spin has no yield point and is
// deadlock-free under systematic testing only because -1 is never observable
// there.
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
    asio_event_t* ev = msg->event;

    switch(msg->flags)
    {
      case ASIO_DISPOSABLE:
        pony_asio_event_send(ev, ASIO_DISPOSABLE, 0);
        break;

      case ASIO_CANCEL_SIGNAL:
      {
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
          // ours to do.
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
              // remove the kevent registration. Teardown errors are benign
              // (matching unsubscribe's convention), so the EV_DELETE is
              // unchecked.
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

// Single function for resubscribing to both reads and writes, matching the
// cross-backend ABI declared in event.h (epoll/emscripten/sock_notify provide
// it too). kqueue re-arms each direction with its own kevent, so this just
// delegates to the per-direction re-arms; each is gated on readable/writeable.
PONY_API void pony_asio_event_resubscribe(asio_event_t* ev)
{
  pony_asio_event_resubscribe_read(ev);
  pony_asio_event_resubscribe_write(ev);
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
    // Under systematic testing execution is serialized to one thread at a time,
    // so any real wait here stalls the whole program while it is our turn. Poll
    // instead: report whatever is already ready and hand our turn straight back.
    // (A normal build leaves timeout NULL and kevent blocks until the kernel has
    // an event.)
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 0;
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

    // Register synchronously so both the OS registration and the subscriber
    // slot are active before subscribe returns — otherwise a raise executed
    // right after subscribing could take the default disposition (process
    // death) or miss this subscriber. See the protocol comment at
    // signal_subscribers_t.
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
          // First subscriber. sigaction goes first — EVFILT_SIGNAL needs a
          // non-terminating disposition in place before registration; SIG_IGN
          // prevents default termination while EVFILT_SIGNAL still detects
          // the signal. The kevent registration is checked via EV_RECEIPT,
          // and the publish comes last so "subscribe returned" implies the
          // signal is being detected.
          struct sigaction new_action;
          new_action.sa_handler = SIG_IGN;
#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
          if(sig == PONY_SCHED_SLEEP_WAKE_SIGNAL)
            new_action.sa_handler = empty_signal_handler;
#endif
          sigemptyset(&new_action.sa_mask);

          // ask to restart interrupted syscalls to match `signal` behavior
          new_action.sa_flags = SA_RESTART;

          if(sigaction(sig, &new_action, NULL) == -1)
          {
            // The OS refused the disposition change; nothing is installed
            // yet, so just reset the state and report.
            atomic_store_explicit(&subs->registered, 0,
              memory_order_seq_cst);
            pony_asio_event_send(ev, ASIO_ERROR, SIGNAL_ERR_REFUSED);
            return;
          }

          struct kevent event;
          EV_SET(&event, sig, EVFILT_SIGNAL, EV_ADD | EV_RECEIPT | EV_CLEAR,
            0, 0, subs);
          struct kevent result;
          int rc = kevent(b->kq, &event, 1, &result, 1, NULL);

          if((rc == -1) || kevent_receipt_has_error(&result, 1))
          {
            // Undo the disposition change and reset the state before
            // reporting, so a later subscribe can retry cleanly.
            new_action.sa_handler = SIG_DFL;
#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
            if(sig == PONY_SCHED_SLEEP_WAKE_SIGNAL)
              new_action.sa_handler = empty_signal_handler;
#endif
            sigaction(sig, &new_action, NULL);
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
        // This wait is very brief — a sigaction, a kevent, and one atomic
        // store.
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
