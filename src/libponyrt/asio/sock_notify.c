#define PONY_WANT_ATOMIC_DEFS

#include "event.h"
#include "asio.h"

#ifdef ASIO_USE_SOCK_NOTIFY

#include "../actor/messageq.h"
#include "../lang/stdfd.h"
#include "../mem/pool.h"
#include "../sched/cpu.h"
#include "../sched/scheduler.h"
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
// retained buffers), so there is no foreign-thread completion delivery. The
// completion port here is only the notification queue (epoll-fd's role), not
// an overlapped-completion port.
//
// PSN is gated in <winsock2.h> on NTDDI_VERSION >= NTDDI_WIN10_MN. We rely on
// the SDK's default version (which exceeds the floor; forcing the macro lower
// could hide other APIs ponyc uses). This guard turns a too-low *explicit*
// setting into a clear error rather than an "undeclared identifier" on PSN.
#if defined(NTDDI_VERSION) && (NTDDI_VERSION < NTDDI_WIN10_MN)
#  error "Windows asio backend requires NTDDI_VERSION >= NTDDI_WIN10_MN (Windows 11 / Server 2022, build 20348)"
#endif

// Requests to start/stop timers, watch stdin, and (un)register signals go
// through a request queue handled on the asio background thread.

#define MAX_SIGNAL 32

// Completion-key sentinels for the non-socket packets we post to the port. Real
// socket events use the heap `asio_event_t*` as the completion key; a heap
// pointer is never one of these small integers, and `asio_event_t.magic`
// (a self-pointer) corroborates a real event.
#define KEY_WAKEUP ((ULONG_PTR)1)  // request queue has work / stop requested
#define KEY_STDIN  ((ULONG_PTR)2)  // stdin readiness (epoch in bytes)
#define KEY_SIGNAL ((ULONG_PTR)3)  // a CRT signal fired (sig number in bytes)

// How long the wait on the port runs before a pipe on stdin is peeked again. It
// is how late a byte on a pipe can be, and how often this thread wakes while a
// program waits on one.
#define STDIN_POLL_MS 10

struct asio_backend_t
{
  HANDLE port;
  PONY_ATOMIC(bool) stop;
  asio_event_t* sighandlers[MAX_SIGNAL];
  messageq_t q;

  // Epoch bumped on each new stdin subscription (ASIO_STDIN_NOTIFY). Each
  // KEY_STDIN packet carries the epoch it was posted in, so the loop drops a
  // packet from an old epoch. Atomic: a pool-thread console callback reads it.
  PONY_ATOMIC(uint32_t) stdin_epoch;
};


enum // Event requests
{
  ASIO_STDIN_NOTIFY = 5,
  ASIO_STDIN_RESUME = 6,
  ASIO_SET_TIMER = 7,
  ASIO_CANCEL_TIMER = 8,
  ASIO_SET_SIGNAL = 9,
  ASIO_CANCEL_SIGNAL = 10,
  ASIO_STDIN_UNSUBSCRIBE = 11
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
  // thread is the ONLY thread that ever sends events.
  // PostQueuedCompletionStatus is thread-safe and touches no event state; the
  // signal number rides in the bytes-transferred field.
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


// Thread-pool callback for the console stdin wait. A console handle cannot be
// associated with a completion port, so the wait is how console readiness
// reaches the port: this callback only posts to the port -- touching no event
// state -- so the asio thread stays the only thread that sends events. The wait
// is registered WT_EXECUTEONLYONCE, so it fires once; we re-arm on resume.
//
// ASIO_STDIN_NOTIFY disarms before it bumps the epoch, so this callback
// finishes before the bump and stamps its packet with its own subscription's
// epoch.
static void CALLBACK stdin_notify_cb(void* arg, BOOLEAN timed_out)
{
  (void)timed_out;
  asio_backend_t* b = (asio_backend_t*)arg;
  uint32_t epoch =
    atomic_load_explicit(&b->stdin_epoch, memory_order_acquire);
  PostQueuedCompletionStatus(b->port, (DWORD)epoch, KEY_STDIN, NULL);
}


// Arm stdin so that one KEY_STDIN packet reaches the dispatch loop, which turns
// it into an ASIO_READ for the subscription it was posted for. Returns false if
// it could not be armed; the caller then sends ASIO_ERROR. With neither the
// packet nor the error, no ASIO_READ reaches the subscriber, its noisy event
// stays subscribed, and the program never quiesces.
//
// A console input handle is signaled while unread input records are queued, so
// a wait on it fires when there is input to read. A pipe has no such signaled
// state, and RegisterWaitForSingleObject does not accept one: the documented
// object list is consoles, events, mutexes, semaphores, timers, processes, and
// threads. A wait on a pipe fires as soon as the previous read completed,
// whether or not any data arrived, and after a read fails on a broken pipe the
// wait was observed never to fire again.
//
// So a pipe is peeked, which says what is in it without taking it and without
// waiting. There is nothing to arm and nothing to post: the dispatch loop peeks
// on every pass and is the one thing that sends the subscriber an ASIO_READ.
// Two things sending it would send the subscriber a read for the same bytes
// twice, and each read resumes, so the reads would multiply.
//
// A read of a file, or of NUL, returns at once. Post the packet; the read then
// returns the data or the end of the input.
static bool arm_stdin(asio_backend_t* b, HANDLE stdin_handle,
  HANDLE* stdin_wait, stdin_kind_t kind)
{
  switch(kind)
  {
    case STDIN_CONSOLE:
      return RegisterWaitForSingleObject(stdin_wait, stdin_handle,
        stdin_notify_cb, b, INFINITE, WT_EXECUTEONLYONCE) != 0;

    case STDIN_PIPE:
      return true;

    case STDIN_OTHER:
    {
      uint32_t epoch =
        atomic_load_explicit(&b->stdin_epoch, memory_order_acquire);
      return PostQueuedCompletionStatus(b->port, (DWORD)epoch, KEY_STDIN, NULL)
        != 0;
    }
  }

  return false;
}


static void disarm_stdin(HANDLE* stdin_wait)
{
  if(*stdin_wait != NULL)
  {
    UnregisterWaitEx(*stdin_wait, INVALID_HANDLE_VALUE);
    *stdin_wait = NULL;
  }
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
  HANDLE stdin_wait = NULL;  // thread-pool wait registration for console stdin
  stdin_kind_t stdin_kind = ponyint_stdin_kind();

  // An ASIO_READ has gone to the stdin subscriber and the read it asks for has
  // not come back yet. A pipe is not peeked while one is outstanding: the bytes
  // are still in it, so peeking would send the subscriber a second ASIO_READ
  // for them. The read clears this by resuming.
  bool stdin_reading = false;

  // Per-wakeup dequeue batch. MAX_EVENTS is the readiness batch size shared
  // with epoll/kqueue; here the one port also carries the wakeup/stdin/signal
  // sentinels, so it bounds those per batch too. It's a throughput knob, not a
  // correctness bound -- anything not dequeued this pass is dequeued next.
  OVERLAPPED_ENTRY entries[MAX_EVENTS];

  while(!atomic_load_explicit(&b->stop, memory_order_acquire))
  {
    // A pipe carries no readiness signal, so the only way to learn it has data
    // is to look. The wait on the port takes a timeout while a program is
    // subscribed to a pipe on stdin and is waiting for what comes next, and the
    // peek happens after the wait, below. A program not reading a pipe on stdin
    // waits on the port with no timeout.
    DWORD wait_ms = INFINITE;

    if((stdin_event != NULL) && (stdin_kind == STDIN_PIPE) && !stdin_reading)
      wait_ms = STDIN_POLL_MS;

    ULONG count = 0;
    BOOL ok = GetQueuedCompletionStatusEx(b->port, entries, MAX_EVENTS, &count,
      wait_ms, TRUE);

    if(!ok && (GetLastError() == WAIT_TIMEOUT))
    {
      // Nothing on the port. The peek below is what this wait was for.
      count = 0;
      ok = TRUE;
    }

    if(!ok)
    {
      // WAIT_IO_COMPLETION: an APC (e.g. a timer fire) ran during the alertable
      // wait -- just loop. Any other error: loop and re-wait.
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
          asio_event_t* ev = b->sighandlers[sig];

          if(ev != NULL)
            pony_asio_event_send(ev, ASIO_SIGNAL, 1);
        }

        continue;
      }
      else if(key == KEY_STDIN)
      {
        // Send ASIO_READ to the subscriber. A console has input queued; a file
        // read returns at once. A pipe does not post this packet -- the peek
        // below drives it -- but a leftover packet may still arrive here.
        //
        // Send only the current subscriber's own packet: a packet posted for an
        // earlier subscriber, dequeued after a later one replaced it, carries a
        // stale epoch and is dropped. Dropping leaves stdin_reading
        // untouched, so the current subscriber's outstanding read is not
        // disturbed. This branch never sends on a freed event: the request
        // drain below clears stdin_event before disposing of the event, and
        // runs after this loop.
        uint32_t pkt_epoch = (uint32_t)entries[i].dwNumberOfBytesTransferred;

        if((stdin_event != NULL) &&
          (pkt_epoch ==
            atomic_load_explicit(&b->stdin_epoch, memory_order_acquire)))
        {
          stdin_reading = true;
          pony_asio_event_send(stdin_event, ASIO_READ, 0);
        }

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
          // Start watching stdin for this event. Unsubscribe is a separate
          // request (ASIO_STDIN_UNSUBSCRIBE), so ev is always non-NULL here.
          //
          // Disarm the previous wait before bumping the epoch: disarming
          // waits for a pending or running console callback to complete, so the
          // old subscriber's callback stamps its packet with the old
          // epoch, not the new one. The bump then makes any leftover
          // packet from the old subscriber stale; arm the new subscriber under
          // the new epoch.
          pony_assert(ev != NULL);
          stdin_event = ev;
          stdin_reading = false;
          disarm_stdin(&stdin_wait);
          atomic_fetch_add_explicit(&b->stdin_epoch, 1,
            memory_order_release);

          if(!arm_stdin(b, stdin_handle, &stdin_wait, stdin_kind))
            pony_asio_event_send(stdin_event, ASIO_ERROR, 0);
          break;

        case ASIO_STDIN_UNSUBSCRIBE:
          // Stop watching stdin and dispose the event. Clear stdin_event and
          // unregister the wait first: once the asio thread holds no reference
          // to the event, the owning actor can free it with no stdin readiness
          // still in flight to reach it. Mirrors the timer and signal cancels,
          // which null their own references before disposing.
          stdin_event = NULL;
          stdin_reading = false;
          disarm_stdin(&stdin_wait);

          ev->flags = ASIO_DISPOSABLE;
          pony_asio_event_send(ev, ASIO_DISPOSABLE, 0);
          break;

        case ASIO_STDIN_RESUME:
          // Input has been read; resume watching stdin.
          stdin_reading = false;

          if(stdin_event != NULL)
          {
            disarm_stdin(&stdin_wait);

            if(!arm_stdin(b, stdin_handle, &stdin_wait, stdin_kind))
              pony_asio_event_send(stdin_event, ASIO_ERROR, 0);
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

        case ASIO_SET_SIGNAL:
        {
          int sig = (int)ev->nsec;

          if(b->sighandlers[sig] == NULL)
          {
            b->sighandlers[sig] = ev;
            signal(sig, signal_handler);
          }
          break;
        }

        case ASIO_CANCEL_SIGNAL:
        {
          int sig = (int)ev->nsec;

          if(b->sighandlers[sig] == ev)
          {
            b->sighandlers[sig] = NULL;
            signal(sig, SIG_DFL);
          }

          ev->flags = ASIO_DISPOSABLE;
          pony_asio_event_send(ev, ASIO_DISPOSABLE, 0);
          break;
        }

        default:  // Something's gone very wrong if we reach here.
          break;
      }
    }

    // Look in the pipe. Something in it, or the end of it, is an ASIO_READ for
    // the subscriber, which reads it and resumes. Nothing in it is nothing to
    // do, and the wait above runs the timeout again.
    if((stdin_event != NULL) && (stdin_kind == STDIN_PIPE) && !stdin_reading &&
      ponyint_stdin_pipe_ready())
    {
      stdin_reading = true;
      pony_asio_event_send(stdin_event, ASIO_READ, 0);
    }
  }

  disarm_stdin(&stdin_wait);

  CloseHandle(b->port);
  ponyint_messageq_destroy(&b->q, true);
  POOL_FREE(asio_backend_t, b);

  TRACING_THREAD_STOP();

  ponyint_unregister_thread();
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
    if(ev->nsec < MAX_SIGNAL)
      send_request(ev, ASIO_SET_SIGNAL);
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
    // Unsubscribe from stdin. Route disposal through the request queue, like
    // the timer and signal cancels above (and like epoll and kqueue). The asio
    // thread clears stdin_event and unregisters the wait before disposing, so
    // it never sends on an event the owning actor has already freed.
    send_request(ev, ASIO_STDIN_UNSUBSCRIBE);
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
        // will arrive to drive the deferred close + disposal. Dispose
        // synchronously here on the owning actor's thread, as the timer/signal
        // branches do. Without this, the event would leak and the actor would
        // never quiesce, matching epoll's unconditional disposal.
        closesocket((SOCKET)ev->fd);
        ev->fd = -1;
        ev->flags = ASIO_DISPOSABLE;
        pony_asio_event_send(ev, ASIO_DISPOSABLE, 0);
      }
    }
  }
}


#endif
