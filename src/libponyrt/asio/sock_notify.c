#define PONY_WANT_ATOMIC_DEFS

#include "event.h"
#include "asio.h"

#ifdef ASIO_USE_SOCK_NOTIFY

#include "../actor/messageq.h"
#include "../mem/pool.h"
#include "../sched/cpu.h"
#include "../sched/scheduler.h"
#include "../sched/systematic_testing.h"
#include "../tracing/tracing.h"
#include "ponyassert.h"
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <winsock2.h>

// The Windows asio backend uses readiness notifications via
// `ProcessSocketNotifications` (PSN), the documented Winsock socket-state
// notification facility (Windows 11 / Server 2022, build 20348). This is the
// same readiness model the epoll and kqueue backends use: the asio thread owns
// a completion port, blocks on it, and turns "socket became ready" packets into
// ASIO_READ/ASIO_WRITE events with one-shot + readable/writeable gating. recv/
// send are then synchronous non-blocking calls with our own buffers (no kernel-
// retained buffers), so there is no foreign-thread completion delivery and no
// liveness token. The completion port here is only the notification queue
// (epoll-fd's role), not an overlapped-completion port.
//
// PSN is gated in <winsock2.h> on NTDDI_VERSION >= NTDDI_WIN10_MN. We rely on
// the SDK's default version (which exceeds the floor; forcing the macro lower
// could hide other APIs ponyc uses). This guard turns a too-low *explicit*
// setting into a clear error rather than an "undeclared identifier" on PSN.
#if defined(NTDDI_VERSION) && (NTDDI_VERSION < NTDDI_WIN10_MN)
#  error "Windows asio backend requires NTDDI_VERSION >= NTDDI_WIN10_MN (Windows 11 / Server 2022, build 20348)"
#endif

// Requests to start/stop timers, watch stdin, and unregister signals go
// through a request queue handled on the asio background thread. Signal
// registration is synchronous on the subscribing thread (see the protocol
// comment below).

#define MAX_SIGNAL 32
// Coupled: the 16-subscriber cap is documented API behavior — see
// .known-couplings/signal-subscriber-cap.md before changing it.
#define MAX_SIGNAL_SUBSCRIBERS 16

// Signal registration protocol (same shape in epoll.c, kqueue.c, and here).
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
// happens only after the OS handler install, so "subscribe returned" implies
// the handler is active. This makes registration and cancel race-free;
// signal-context delivery keeps its pre-existing benign race (the CRT
// handler's self-re-arm can race a concurrent teardown's signal(SIG_DFL),
// which existed before this design too).
typedef struct signal_subscribers_t {
  PONY_ATOMIC(int) registered;  // 0=no, -1=in-progress, 1=yes
  PONY_ATOMIC(asio_event_t*) subscribers[MAX_SIGNAL_SUBSCRIBERS];
} signal_subscribers_t;

// Completion-key sentinels for the non-socket packets we post to the port. Real
// socket events use the heap `asio_event_t*` as the completion key; a heap
// pointer is never one of these small integers, and `asio_event_t.magic`
// (a self-pointer) corroborates a real event.
#define KEY_WAKEUP ((ULONG_PTR)1)  // request queue has work / stop requested
#define KEY_STDIN  ((ULONG_PTR)2)  // console stdin became readable
#define KEY_SIGNAL ((ULONG_PTR)3)  // a CRT signal fired (sig number in bytes)

struct asio_backend_t
{
  HANDLE port;
  PONY_ATOMIC(bool) stop;
  signal_subscribers_t sighandlers[MAX_SIGNAL];
  messageq_t q;
};


enum // Event requests
{
  ASIO_STDIN_NOTIFY = 5,
  ASIO_STDIN_RESUME = 6,
  ASIO_SET_TIMER = 7,
  ASIO_CANCEL_TIMER = 8,
  ASIO_CANCEL_SIGNAL = 10  // matches epoll.c/kqueue.c
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

  PostQueuedCompletionStatus(b->port, 0, KEY_WAKEUP, NULL);
}


static void signal_handler(int sig)
{
  if(sig >= MAX_SIGNAL)
    return;

  // Reset the signal handler (one-shot CRT semantics).
  signal(sig, signal_handler);

  // POSIX (epoll.c) routes signals through the asio thread: the handler only
  // wakes that thread, which then sends the event. We mirror that so the asio
  // thread is the ONLY thread that ever sends events -- the property that makes
  // the old foreign-thread liveness token unnecessary. PostQueuedCompletionStatus
  // is thread-safe and touches no event state; the signal number rides in the
  // bytes-transferred field.
  asio_backend_t* b = ponyint_asio_get_backend();

  if(b != NULL)
    PostQueuedCompletionStatus(b->port, (DWORD)sig, KEY_SIGNAL, NULL);
}


static void CALLBACK timer_fire(void* arg, DWORD timer_low, DWORD timer_high)
{
  (void)timer_low;
  (void)timer_high;
  // A waitable-timer APC fired on the asio thread (the port wait is alertable).
  pony_asio_event_send((asio_event_t*)arg, ASIO_TIMER, 0);
}


// Thread-pool callback that watches the console stdin handle, which cannot be
// associated with a completion port. It only posts to the port -- touching no
// event state -- preserving the asio-thread-only-sends invariant. The wait is
// registered WT_EXECUTEONLYONCE, so it fires once; we re-arm on resume.
static void CALLBACK stdin_notify_cb(void* arg, BOOLEAN timed_out)
{
  (void)timed_out;
  asio_backend_t* b = (asio_backend_t*)arg;
  PostQueuedCompletionStatus(b->port, 0, KEY_STDIN, NULL);
}


asio_backend_t* ponyint_asio_backend_init()
{
  asio_backend_t* b = POOL_ALLOC(asio_backend_t);
  memset(b, 0, sizeof(asio_backend_t));
  ponyint_messageq_init(&b->q);

  b->port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
  b->stop = false;

  if(b->port == NULL)
  {
    ponyint_messageq_destroy(&b->q, true);
    POOL_FREE(asio_backend_t, b);
    return NULL;
  }

  return b;
}


void ponyint_asio_backend_final(asio_backend_t* b)
{
  atomic_store_explicit(&b->stop, true, memory_order_relaxed);
  PostQueuedCompletionStatus(b->port, 0, KEY_WAKEUP, NULL);
}


// Map an event's ASIO_READ/ASIO_WRITE flags to a PSN registration filter. We
// add HANGUP whenever reads are wanted (peer-close detection, like EPOLLRDHUP).
// ERR and REMOVE are delivered regardless of the filter (per the SDK docs).
static UINT16 sock_notify_filter(asio_event_t* ev)
{
  UINT16 filter = SOCK_NOTIFY_REGISTER_EVENT_NONE;

  if(ev->flags & ASIO_READ)
    filter |= SOCK_NOTIFY_REGISTER_EVENT_IN | SOCK_NOTIFY_REGISTER_EVENT_HANGUP;

  if(ev->flags & ASIO_WRITE)
    filter |= SOCK_NOTIFY_REGISTER_EVENT_OUT;

  return filter;
}


// Trigger mode mirrors epoll exactly:
//   ASIO_ONESHOT set   -> level-triggered one-shot  (== EPOLLONESHOT)
//   ASIO_ONESHOT clear -> edge-triggered persistent (== EPOLLET)
// The listener subscribes without ASIO_ONESHOT, so a paused accept loop refires
// only on new arrivals instead of busy-spinning.
static UINT8 sock_notify_trigger(asio_event_t* ev)
{
  if(ev->flags & ASIO_ONESHOT)
    return SOCK_NOTIFY_TRIGGER_LEVEL | SOCK_NOTIFY_TRIGGER_ONESHOT;

  return SOCK_NOTIFY_TRIGGER_EDGE | SOCK_NOTIFY_TRIGGER_PERSISTENT;
}


// Issue a single ProcessSocketNotifications control op for a socket event. Like
// epoll_ctl, this is thread-safe and called from the owning actor's thread.
// Returns true on success.
static bool sock_notify_ctl(asio_event_t* ev, UINT16 filter, UINT8 op,
  UINT8 trigger)
{
  asio_backend_t* b = ponyint_asio_get_backend();
  pony_assert(b != NULL);

  SOCK_NOTIFY_REGISTRATION reg;
  memset(&reg, 0, sizeof(reg));
  reg.socket = (SOCKET)ev->fd;
  reg.completionKey = ev;
  reg.eventFilter = filter;
  reg.operation = op;
  reg.triggerFlags = trigger;

  DWORD r = ProcessSocketNotifications(b->port, 1, &reg, 0, 0, NULL, NULL);
  return (r == ERROR_SUCCESS) && (reg.registrationResult == ERROR_SUCCESS);
}


DECLARE_THREAD_FN(ponyint_asio_backend_dispatch)
{
  ponyint_cpu_affinity(ponyint_asio_get_cpu());
  ponyint_register_asio_thread();
  asio_backend_t* b = (asio_backend_t*)arg;
  pony_assert(b != NULL);

  asio_event_t* stdin_event = NULL;
  HANDLE stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
  HANDLE stdin_wait = NULL;  // thread-pool wait registration for stdin

#if defined(USE_SYSTEMATIC_TESTING)
  // sleep thread until we're ready to start processing
  SYSTEMATIC_TESTING_WAIT_START(ponyint_asio_get_backend_tid(), ponyint_asio_get_backend_sleep_object());
#endif

  // Per-wakeup dequeue batch. MAX_EVENTS is the readiness batch size shared
  // with epoll/kqueue; here the one port also carries the wakeup/stdin/signal
  // sentinels, so it bounds those per batch too. It's a throughput knob, not a
  // correctness bound -- anything not dequeued this pass is dequeued next.
  OVERLAPPED_ENTRY entries[MAX_EVENTS];

  while(!atomic_load_explicit(&b->stop, memory_order_acquire))
  {
    DWORD wait_ms = INFINITE;
#if defined(USE_SYSTEMATIC_TESTING)
    // Under systematic testing execution is serialized to one thread at a time,
    // so any real wait here stalls the whole program while it is our turn. Poll
    // instead: report whatever is already ready and hand our turn straight back.
    // (A normal build leaves wait_ms at INFINITE and blocks until the port has a
    // completion.)
    wait_ms = 0;
#endif

    ULONG count = 0;
    BOOL ok = GetQueuedCompletionStatusEx(b->port, entries, MAX_EVENTS, &count,
      wait_ms, TRUE);

    SYSTEMATIC_TESTING_YIELD();

    if(!ok)
    {
      // WAIT_IO_COMPLETION: an APC (e.g. a timer fire) ran during the alertable
      // wait -- just loop. WAIT_TIMEOUT: under systematic testing's zero-timeout
      // poll, the normal empty-pass return. Any other error: loop and re-wait.
      continue;
    }

    // Socket events whose REMOVE packet we saw this batch. We defer the
    // close + ASIO_DISPOSABLE to *after* the batch loop: sending ASIO_DISPOSABLE
    // lets the owning actor free the event concurrently on its scheduler
    // thread, so disposing inline would risk a later same-batch entry
    // dereferencing a freed event. Deferral mirrors epoll.c, where disposal
    // happens in handle_queue after the readiness loop.
    asio_event_t* to_dispose[MAX_EVENTS];
    ULONG dispose_count = 0;

    for(ULONG i = 0; i < count; i++)
    {
      ULONG_PTR key = entries[i].lpCompletionKey;

      if(key == KEY_WAKEUP)
      {
        // Request queue has work (drained below) or stop was requested.
        continue;
      }
      else if(key == KEY_SIGNAL)
      {
        int sig = (int)entries[i].dwNumberOfBytesTransferred;

        if((sig >= 0) && (sig < MAX_SIGNAL))
        {
          signal_subscribers_t* subs = &b->sighandlers[sig];

          for(size_t j = 0; j < MAX_SIGNAL_SUBSCRIBERS; j++)
          {
            asio_event_t* sub = atomic_load_explicit(
              &subs->subscribers[j], memory_order_acquire);
            if(sub != NULL)
              pony_asio_event_send(sub, ASIO_SIGNAL, 1);
          }
        }

        continue;
      }
      else if(key == KEY_STDIN)
      {
        // Console stdin became readable. Level-triggered: we stay dropped until
        // the reader resumes us (the WT_EXECUTEONLYONCE wait already stopped).
        if(stdin_event != NULL)
          pony_asio_event_send(stdin_event, ASIO_READ, 0);

        continue;
      }

      // Otherwise it's a socket readiness packet; the key is the event.
      // Dereferencing the key is memory-safe because of PSN's terminal-REMOVE
      // guarantee: once a registration's SOCK_NOTIFY_EVENT_REMOVE packet is
      // delivered, no further packet for that registration is delivered, so we
      // never dequeue a packet whose event has been freed (we free only after
      // REMOVE-seen). The magic self-pointer corroborates a real event.
      asio_event_t* ev = (asio_event_t*)key;
      pony_assert(ev->magic == ev);

      uint32_t mask = (uint32_t)SocketNotificationRetrieveEvents(&entries[i]);

      if(mask & SOCK_NOTIFY_EVENT_REMOVE)
      {
        // REMOVE-seen: teardown is drained. Record the event for close +
        // disposal after the batch loop (see to_dispose above). At most one
        // REMOVE packet per event (unsubscribe issues OP_REMOVE at most once),
        // so no event is recorded twice.
        to_dispose[dispose_count++] = ev;
        continue;
      }

      // If unsubscribe has already issued REMOVE for this event, drop any
      // in-flight readiness packet dequeued before the REMOVE packet -- the
      // event is being torn down and must not be dispatched. Safe even when the
      // REMOVE packet is in this same batch: disposal is deferred to after the
      // loop, so ev is not freed yet.
      if(ev->removing)
        continue;

      uint32_t flags = 0;
      bool err_like =
        (mask & (SOCK_NOTIFY_EVENT_HANGUP | SOCK_NOTIFY_EVENT_ERR)) != 0;

      if(ev->flags & ASIO_READ)
      {
        // IN, or a hangup/error condition, surfaces as a read so the
        // synchronous recv can report the close/error (mirrors epoll's
        // EPOLLIN|RDHUP|HUP|ERR read arm). One-shot gating as in epoll.c.
        if((mask & SOCK_NOTIFY_EVENT_IN) || err_like)
        {
          if(((ev->flags & ASIO_ONESHOT) && !ev->readable) ||
            !(ev->flags & ASIO_ONESHOT))
          {
            ev->readable = true;
            flags |= ASIO_READ;
          }
        }
      }

      if(ev->flags & ASIO_WRITE)
      {
        // OUT, or ERR, surfaces as a write. ERR is what PSN delivers on a
        // failed connect (no OUT), and the stdlib's connect-completion arm
        // keys on the writeable flag to check SO_ERROR -- so ERR must reach it.
        if((mask & SOCK_NOTIFY_EVENT_OUT) || (mask & SOCK_NOTIFY_EVENT_ERR))
        {
          if(((ev->flags & ASIO_ONESHOT) && !ev->writeable) ||
            !(ev->flags & ASIO_ONESHOT))
          {
            ev->writeable = true;
            flags |= ASIO_WRITE;
          }
        }
      }

      if(flags != 0)
        pony_asio_event_send(ev, flags, 0);
    }

    // Now that the whole batch is processed and no entry can still reference an
    // event, close + dispose the events whose REMOVE we saw. The backend owns
    // the close for subscribed sockets (closesocket emits no REMOVE, so closing
    // earlier would strand the handshake); the owning actor frees the event on
    // ASIO_DISPOSABLE.
    for(ULONG i = 0; i < dispose_count; i++)
    {
      asio_event_t* ev = to_dispose[i];

      if(ev->fd != -1)
        closesocket((SOCKET)ev->fd);

      ev->flags = ASIO_DISPOSABLE;
      pony_asio_event_send(ev, ASIO_DISPOSABLE, 0);
    }

    // Drain the request queue every iteration (like epoll.c's handle_queue);
    // a KEY_WAKEUP packet is just the nudge that woke us.
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
        case ASIO_STDIN_NOTIFY:
          // Who to notify about stdin events has changed.
          stdin_event = ev;

          if(stdin_wait != NULL)
          {
            UnregisterWaitEx(stdin_wait, INVALID_HANDLE_VALUE);
            stdin_wait = NULL;
          }

          if(stdin_event != NULL)  // Someone wants stdin; start watching it.
            RegisterWaitForSingleObject(&stdin_wait, stdin_handle,
              stdin_notify_cb, b, INFINITE, WT_EXECUTEONLYONCE);
          break;

        case ASIO_STDIN_RESUME:
          // Console input has been read; resume watching stdin.
          if(stdin_event != NULL)
          {
            if(stdin_wait != NULL)
            {
              UnregisterWaitEx(stdin_wait, INVALID_HANDLE_VALUE);
              stdin_wait = NULL;
            }

            RegisterWaitForSingleObject(&stdin_wait, stdin_handle,
              stdin_notify_cb, b, INFINITE, WT_EXECUTEONLYONCE);
          }
          break;

        case ASIO_SET_TIMER:
        {
          // Windows timer resolution is 100ns, adjust given time.
          int64_t dueTime = -(int64_t)ev->nsec / 100;
          LARGE_INTEGER liDueTime;
          liDueTime.LowPart = (DWORD)(dueTime & 0xFFFFFFFF);
          liDueTime.HighPart = (LONG)(dueTime >> 32);

          SetWaitableTimer(ev->timer, &liDueTime, 0, timer_fire,
            (void*)ev, FALSE);
          break;
        }

        case ASIO_CANCEL_TIMER:
        {
          CancelWaitableTimer(ev->timer);
          CloseHandle(ev->timer);
          ev->timer = NULL;

          // Now that we've cancelled, no more fire APCs can happen for this
          // timer, so it's safe to dispose now.
          ev->flags = ASIO_DISPOSABLE;
          pony_asio_event_send(ev, ASIO_DISPOSABLE, 0);
          break;
        }

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
              atomic_store_explicit(&subs->subscribers[i], NULL,
                memory_order_release);
              break;
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
                // Last subscriber is gone: restore default handling.
                signal(sig, SIG_DFL);
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

        default:  // Something's gone very wrong if we reach here.
          break;
      }
    }
  }

  if(stdin_wait != NULL)
    UnregisterWaitEx(stdin_wait, INVALID_HANDLE_VALUE);

  CloseHandle(b->port);
  ponyint_messageq_destroy(&b->q, true);
  POOL_FREE(asio_backend_t, b);

  SYSTEMATIC_TESTING_STOP_THREAD();
  TRACING_THREAD_STOP();

  pony_unregister_thread();
  return NULL;
}


// Called from stdfd.c to resume waiting on stdin after a read.
void ponyint_sock_notify_resume_stdin()
{
  send_request(NULL, ASIO_STDIN_RESUME);
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

  if((ev->flags & ASIO_TIMER) != 0)
  {
    // Need to start a timer. We create it here but start it on the background
    // thread, because that's where we want the fire APC to be delivered.
    ev->timer = CreateWaitableTimer(NULL, FALSE, NULL);
    send_request(ev, ASIO_SET_TIMER);
  } else if((ev->flags & ASIO_SIGNAL) != 0) {
    if(ev->nsec >= MAX_SIGNAL)
    {
      // Out of range (only reachable through raw FFI — ValidSignal caps
      // well below MAX_SIGNAL). Report it so the handler auto-disposes
      // instead of stranding, per the documented contract.
      pony_asio_event_send(ev, ASIO_ERROR, 0);
      return;
    }

    int sig = (int)ev->nsec;
    signal_subscribers_t* subs = &b->sighandlers[sig];

    // Register synchronously so both the CRT handler and the subscriber slot
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
          // First subscriber: install the CRT handler, then publish. The
          // publish comes last so "subscribe returned" implies the handler
          // is installed.
          if(signal(sig, signal_handler) == SIG_ERR)
          {
            // The CRT refused the handler. Reset the state and report, so
            // the failure surfaces as an auto-disposed handler instead of a
            // registration that never delivers.
            atomic_store_explicit(&subs->registered, 0,
              memory_order_seq_cst);
            pony_asio_event_send(ev, ASIO_ERROR, 0);
            return;
          }

          atomic_store_explicit(&subs->registered, 1, memory_order_seq_cst);
          state = 1;
        }
      }

      if(state != 1)
      {
        // Another thread is mid-install or mid-teardown. This wait is very
        // brief — one CRT signal() call and an atomic store.
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
        pony_asio_event_send(ev, ASIO_ERROR, 0);
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
  } else if(ev->fd == 0) {
    // Subscribe to stdin.
    send_request(ev, ASIO_STDIN_NOTIFY);
  } else {
    // Socket: register for readiness notifications on our port. Like
    // epoll_ctl(ADD), this runs on the owning actor's thread.
    if(!sock_notify_ctl(ev, sock_notify_filter(ev), SOCK_NOTIFY_OP_ENABLE,
      sock_notify_trigger(ev)))
      pony_asio_event_send(ev, ASIO_ERROR, 0);
  }
}


// Single function for resubscribing to both reads and writes for a one-shot
// socket event. Re-arms via PSN OP_ENABLE (verified to re-enable a fired
// one-shot registration), gated on readable/writeable exactly like epoll.c.
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

  UINT16 filter = SOCK_NOTIFY_REGISTER_EVENT_NONE;

  // if the event is supposed to be listening for write notifications
  // and it is currently not writeable
  if((ev->flags & ASIO_WRITE) && !ev->writeable)
    filter |= SOCK_NOTIFY_REGISTER_EVENT_OUT;

  // if the event is supposed to be listening for read notifications
  // and it is currently not readable
  if((ev->flags & ASIO_READ) && !ev->readable)
    filter |= SOCK_NOTIFY_REGISTER_EVENT_IN | SOCK_NOTIFY_REGISTER_EVENT_HANGUP;

  // only resubscribe if there is something to resubscribe to
  if(filter != SOCK_NOTIFY_REGISTER_EVENT_NONE)
  {
    if(!sock_notify_ctl(ev, filter, SOCK_NOTIFY_OP_ENABLE,
      SOCK_NOTIFY_TRIGGER_LEVEL | SOCK_NOTIFY_TRIGGER_ONESHOT))
      pony_asio_event_send(ev, ASIO_ERROR, 0);
  }
}


// Kept to maintain backwards compatibility so folks don't have to change their
// code to use `pony_asio_event_resubscribe` immediately.
PONY_API void pony_asio_event_resubscribe_write(asio_event_t* ev)
{
  pony_asio_event_resubscribe(ev);
}

// Kept to maintain backwards compatibility so folks don't have to change their
// code to use `pony_asio_event_resubscribe` immediately.
PONY_API void pony_asio_event_resubscribe_read(asio_event_t* ev)
{
  pony_asio_event_resubscribe(ev);
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

  if((ev->flags & ASIO_TIMER) != 0)
  {
    // Need to restart a timer. That must be done on the background thread,
    // because that's where we want the fire APC to be delivered.
    ev->nsec = nsec;
    send_request(ev, ASIO_SET_TIMER);
  }
}


PONY_API void pony_asio_event_unsubscribe(asio_event_t* ev)
{
  if((ev == NULL) ||
    (ev->flags == ASIO_DISPOSABLE) ||
    (ev->flags == ASIO_DESTROYED))
    return;

  asio_backend_t* b = ponyint_asio_get_backend();
  pony_assert(b != NULL);

  if((ev->flags & ASIO_TIMER) != 0)
  {
    // Need to cancel a timer. Route the cancel through the queue too, so it
    // can't overtake a pending set.
    send_request(ev, ASIO_CANCEL_TIMER);
  } else if((ev->flags & ASIO_SIGNAL) != 0) {
    if(ev->nsec < MAX_SIGNAL)
    {
      send_request(ev, ASIO_CANCEL_SIGNAL);
    } else {
      ev->flags = ASIO_DISPOSABLE;
      pony_asio_event_send(ev, ASIO_DISPOSABLE, 0);
    }
  } else if(ev->fd == 0) {
    // Unsubscribe from stdin. These paths are driven from the asio thread and
    // never had foreign-thread races, so dispose synchronously (as before).
    send_request(NULL, ASIO_STDIN_NOTIFY);

    ev->flags = ASIO_DISPOSABLE;
    pony_asio_event_send(ev, ASIO_DISPOSABLE, 0);
  } else {
    // Socket. Issue PSN REMOVE and mark the event "removing". Disposal is
    // DEFERRED: the dispatch loop sends ASIO_DISPOSABLE (and closes ev->fd)
    // only when it later dequeues the SOCK_NOTIFY_EVENT_REMOVE packet for this
    // event. closesocket emits no REMOVE, so the close must not precede
    // REMOVE-seen -- hence the backend owns it. Until then ev->fd and the event
    // must stay alive. Issue REMOVE at most once per event; a second
    // unsubscribe (e.g. the losing connect-event path) is then a no-op.
    if(!ev->removing)
    {
      ev->removing = true;

      if(!sock_notify_ctl(ev, SOCK_NOTIFY_REGISTER_EVENT_NONE,
        SOCK_NOTIFY_OP_REMOVE, 0))
      {
        // The REMOVE control call failed, so no SOCK_NOTIFY_EVENT_REMOVE packet
        // will arrive to drive the deferred close + disposal. A failed REMOVE
        // means the socket isn't registered with the port, so no readiness
        // packet for it is in flight on the asio thread -- it is safe to dispose
        // synchronously here on the owning actor's thread (as the timer/signal/
        // stdin branches do). Without this, the event would leak and the actor
        // would never quiesce, matching epoll's unconditional disposal.
        closesocket((SOCKET)ev->fd);
        ev->fd = -1;
        ev->flags = ASIO_DISPOSABLE;
        pony_asio_event_send(ev, ASIO_DISPOSABLE, 0);
      }
    }
  }
}


#endif
