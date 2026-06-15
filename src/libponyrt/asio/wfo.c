#define PONY_WANT_ATOMIC_DEFS

#include "asio.h"
#include "event.h"
#ifdef ASIO_USE_WFO

#include "../mem/pool.h"
#include "../sched/cpu.h"
#include "../sched/scheduler.h"
#include "../sched/systematic_testing.h"
#include "../tracing/tracing.h"
#include "ponyassert.h"
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>
#include <os/kernel/OS.h>
#include <os/support/SupportDefs.h>

#define MAX_SIGNAL __MAX_SIGNO + 1

enum // Internal WFO event requests
{
//ASIO_DISPOSABLE = 0,
  WFO_WAKEUP = 1,
  WFO_TIMER_FIRED = 2,
  WFO_SUBSCRIBE = 3,
  WFO_RESUBSCRIBE = 4,
  WFO_UNSUBSCRIBE = 5,
};

enum // Internal WFO errors
{
  WFO_ERROR_WAIT_LIST_FULL = -1,
  WFO_ERROR_EVENT_ALREADY_SUBSCRIBED = -2,
  WFO_ERROR_INVALID_OBJECT = -3,
  WFO_ERROR_EVENT_MISMATCH = -4,
};

struct asio_backend_t
{
  port_id port;

  object_wait_info wait_list[MAX_EVENTS];
  uint16 wait_events[MAX_EVENTS];
  asio_event_t* events[MAX_EVENTS];
  int32 wait_list_count;

  PONY_ATOMIC(asio_event_t*) sighandlers[MAX_SIGNAL];
};

struct wfo_message_t
{
  asio_event_t* ev;
  uint32_t flags;
};

static status_t wfo_send_request(asio_event_t* ev, int32 req)
{
  asio_backend_t* b = ponyint_asio_get_backend();
  if(b == NULL || b->port < 0)
  {
    // Something tried to send request after backend finalization already happened?
    pony_assert(0);
    return B_SHUTTING_DOWN;
  }

  struct wfo_message_t msg;
  msg.ev = ev;
  // We store event flags as they are at moment of sending message, because
  // wfo_handle_queue might be called after ev->flags are changed in meantime.
  msg.flags = ev->flags;

  return write_port(b->port, req, &msg, sizeof(msg));
}

static void wfo_signal_handler(int sig, void* userData)
{
  if(sig >= MAX_SIGNAL)
    return;

  asio_event_t* ev = (asio_event_t*)userData;

  if(ev == NULL)
    return;

  asio_backend_t* b = ponyint_asio_get_backend();
  if(b == NULL || b->port < 0)
    return;

  pony_asio_event_send(ev, ASIO_SIGNAL, 1);
}

static void wfo_timer_handler(union sigval val)
{
  asio_event_t* ev = (asio_event_t*)val.sival_ptr;

  if(ev == NULL || ev->timerID == NULL)
    return;

  asio_backend_t* b = ponyint_asio_get_backend();
  if(b == NULL || b->port < 0)
    return;

  // We're making Haiku kernel call our `timer_handler` from a "new thread"
  // (or same one for all timer calls, but still not owned by us AFAIK).
  // When it calls us, and we try to call pony_asio_event_send, pony context is not set.
  // When we try to create one, by calling `pony_register_thread`, we may crash,
  // because `this_scheduler` variable does not seem to exist! Like if thread local storage
  // was not set at all?
  // This might be a Haiku-only problem, but still prevents us from simply sending pony event.
  // With examples/timers it worked fine, but with examples/under_pressure it consistently crashes.
//  pony_asio_event_send(ev, ASIO_TIMER, 0);
  // That's why we send request to our own thread, which will in turn send the pony event.
  if(wfo_send_request(ev, WFO_TIMER_FIRED) < B_OK)
  {
    pony_assert(0);
  }
}

#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
static void wfo_empty_signal_handler(int sig)
{
  (void) sig;
}
#endif

int32 wfo_event_add(asio_backend_t* b, asio_event_t* ev, int32 object, uint16 type, uint16 events)
{
  pony_assert(b != NULL);

  int32 index = b->wait_list_count;

  if(index >= (int32)B_COUNT_OF(b->wait_list))
  {
    return WFO_ERROR_WAIT_LIST_FULL;
  }

  if(ev != NULL && ev->wfo_id != -1)
  {
    return WFO_ERROR_EVENT_ALREADY_SUBSCRIBED;
  }

  if(object < 0)
  {
    return WFO_ERROR_INVALID_OBJECT;
  }

  b->wait_list_count += 1;

  b->wait_list[index].object = object;
  b->wait_list[index].type = type;
  b->wait_list[index].events = events;

  b->wait_events[index] = events;

  b->events[index] = ev;

  if(ev != NULL)
    ev->wfo_id = index;

  return index;
}

int32 wfo_event_update(asio_backend_t* b, asio_event_t* ev, int32 index, int32 object, uint16 type, uint16 events)
{
  pony_assert(b != NULL);

  if(index >= MAX_EVENTS || index < 0)
    return wfo_event_add(b, ev, object, type, events);

  object_wait_info* info = &b->wait_list[index];

  info->object = object;
  info->type = type;
  info->events = events;

  b->wait_events[index] = events;

  b->events[index] = ev;

  if(ev != NULL)
    ev->wfo_id = index;

  return index;
}

int32 wfo_event_remove(asio_backend_t* b, asio_event_t* ev)
{
  pony_assert(b != NULL);
  pony_assert(ev != NULL);

  int32 index = ev->wfo_id;
  if(index < 0)
    return -1;

  ev->wfo_id = -1;

  if(b->events[index] != ev)
  {
    return WFO_ERROR_EVENT_MISMATCH;
  }

  int32 last = b->wait_list_count - 1;

  if(b->wait_list_count > 0)
    --b->wait_list_count;

  if(last <= index)
  {
    return 0;
  }

  b->wait_list[index] = b->wait_list[last];
  b->wait_events[index] = b->wait_events[last];
  b->events[index] = b->events[last];

  // We could be called after wait_for_objects modified events,
  // before they were refreshed, so refresh them here.
  b->wait_list[index].events = b->wait_events[index];

  asio_event_t* ev2 = b->events[index];
  if(ev2 != NULL && ev2->wfo_id >= 0)
    ev2->wfo_id = index;

  return 0;
}

static uint16 wfo_events_from_asio_flags(uint32_t flags)
{
  uint16 events = 0;

  // WARNING: This is a hack.
  //          Haiku, AFAIK, does not trigger B_EVENT_PRIORITY_* and B_EVENT_HIGH_PRIORITY_* events.
  //          So we use one of them to keep ONESHOT event valid, when we want to ignore it for now.
  //          Which means that we'll be using either both READ and HIGHT_PRIORITY_READ,
  //          or just the HIGH_PRIORITY_READ.
  //          This allows us to emulate ONESHOT, without a need for dispatch function to
  //          access asio_event_t's readable/writeable in a race condition manner,
  //          and without a need to remove and re-add the event to our wait_list.
  if(flags & ASIO_READ)
  {
    events |= B_EVENT_READ;
    if(flags & ASIO_ONESHOT)
    {
      events |= B_EVENT_HIGH_PRIORITY_READ;
    }
  }
  if(flags & ASIO_WRITE)
  {
    events |= B_EVENT_WRITE;
    if(flags & ASIO_ONESHOT)
    {
      events |= B_EVENT_HIGH_PRIORITY_WRITE;
    }
  }

  return events;
}

static void wfo_handle_queue(asio_backend_t* b)
{
  int32 req;
  struct wfo_message_t msg;
  ssize_t read_size = 0;

  while((read_size = read_port_etc(b->port, &req, &msg, sizeof(msg), B_TIMEOUT, 0)) >= 0)
  {
    if(read_size != sizeof(msg))
    {
      // 0 means it was just a wakeup-type message.
      pony_assert(read_size == 0);
      continue;
    }

    asio_event_t* ev = msg.ev;
    uint32_t flags = msg.flags;
    uint16 events = 0;

    switch(req)
    {
      case ASIO_DISPOSABLE:
        pony_asio_event_send(ev, ASIO_DISPOSABLE, 0);
        break;

      case WFO_TIMER_FIRED:
        if(ev->timerID != NULL)
          pony_asio_event_send(ev, ASIO_TIMER, 0);
        break;

      case WFO_SUBSCRIBE:
        if(ev->fd < 0) continue;

        events = wfo_events_from_asio_flags(flags);

        int32 result = -1;
        if(ev->wfo_id >= 0)
          result = wfo_event_update(b, ev, ev->wfo_id, ev->fd, B_OBJECT_TYPE_FD, events);
        else
          result = wfo_event_add(b, ev, ev->fd, B_OBJECT_TYPE_FD, events);

        if (result < B_OK)
          pony_asio_event_send(ev, ASIO_ERROR, 0);
        break;

      case WFO_RESUBSCRIBE:
        if(ev->fd < 0) continue;

        events = wfo_events_from_asio_flags(flags);

        if(wfo_event_update(b, ev, ev->wfo_id, ev->fd, B_OBJECT_TYPE_FD, events) < B_OK)
          pony_asio_event_send(ev, ASIO_ERROR, 0);
        break;

      case WFO_UNSUBSCRIBE:
        // Don't send ASIO_ERROR on failure — the actor is tearing down anyway.
        wfo_event_remove(b, ev);
        break;

      default: {}
    }
  }
}

asio_backend_t* ponyint_asio_backend_init()
{
  asio_backend_t* b = POOL_ALLOC(asio_backend_t);
  memset(b, 0, sizeof(asio_backend_t));

  b->port = create_port(MAX_EVENTS * 2, "libponyrt asio backend queue");
  if(b->port < B_OK)
  {
    POOL_FREE(asio_backend_t, b);
    return NULL;
  }

  b->wait_list_count = 0;
  wfo_event_add(b, NULL, b->port, B_OBJECT_TYPE_PORT, B_EVENT_READ);

#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
  // Make sure we ignore signals related to scheduler sleeping/waking
  // as the default for those signals is termination.
  struct sigaction new_action;
  new_action.sa_handler = wfo_empty_signal_handler;
  new_action.sa_userdata = NULL;
  sigemptyset (&new_action.sa_mask);

  // Ask to restart interrupted syscalls to match `signal` behavior.
  new_action.sa_flags = SA_RESTART;

  sigaction(PONY_SCHED_SLEEP_WAKE_SIGNAL, &new_action, NULL);
#endif

  return b;
}

void ponyint_asio_backend_final(asio_backend_t* b)
{
  delete_port(b->port);
}

// Single function for resubscribing to both reads and writes for an event.
PONY_API void pony_asio_event_resubscribe(asio_event_t* ev)
{
  // Needs to be a valid event that is one shot enabled.
  if((ev == NULL) ||
    (ev->flags == ASIO_DISPOSABLE) ||
    (ev->flags == ASIO_DESTROYED) ||
    !(ev->flags & ASIO_ONESHOT))
  {
    pony_assert(0);
    return;
  }

  if(wfo_send_request(ev, WFO_RESUBSCRIBE) < B_OK)
  {
    pony_asio_event_send(ev, ASIO_ERROR, 0);
  }
}

// Kept to maintain backwards compatibility so folks don't
// have to change their code to use `pony_asio_event_resubscribe`
// immediately.
PONY_API void pony_asio_event_resubscribe_write(asio_event_t* ev)
{
  pony_asio_event_resubscribe(ev);
}

// Kept to maintain backwards compatibility so folks don't
// have to change their code to use `pony_asio_event_resubscribe`
// immediately.
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

  rename_thread(get_pthread_thread_id(pthread_self()), "wfo::ponyint_asio_backend_dispatch");

#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
  // Make sure we block signals related to scheduler sleeping/waking
  // so they queue up to avoid race conditions.
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, PONY_SCHED_SLEEP_WAKE_SIGNAL);
  pthread_sigmask(SIG_BLOCK, &set, NULL);
#endif

#if defined(USE_SYSTEMATIC_TESTING)
  // Sleep thread until we're ready to start processing.
  SYSTEMATIC_TESTING_WAIT_START(ponyint_asio_get_backend_tid(), ponyint_asio_get_backend_sleep_object());
#endif

  bool should_refresh_events = false;

  while(true)
  {
    int32 wait_count = b->wait_list_count;

    if(should_refresh_events)
    {
      for(int i = 0; i < wait_count; i++) b->wait_list[i].events = b->wait_events[i];
      should_refresh_events = false;
    }

#if defined(USE_SYSTEMATIC_TESTING)
    // Wait only for 10 milliseconds.
    ssize_t result = wait_for_objects_etc(b->wait_list, wait_count, B_RELATIVE_TIMEOUT, 10000);
#else
    // Wait indefinitely.
    ssize_t result = wait_for_objects(b->wait_list, wait_count);
#endif

    SYSTEMATIC_TESTING_YIELD();

    if(result == B_TIMED_OUT || result == B_WOULD_BLOCK || result == B_INTERRUPTED)
    {
      ssize_t count = port_count(b->port);
      if(count == B_BAD_PORT_ID)
      {
        // `ponyint_asio_backend_final` simply deletes b->port, so we know it's time to quit.
        break;
      }
      else if(count < 0)
      {
        // Some unknown error happened!
        pony_assert(0);
      }
      else if(count > 0)
      {
        // Timed out and there are more than 0 messages in queue, so call wfo_handle_queue.
        wfo_handle_queue(b);
      }

      should_refresh_events = true;
      continue;
    }
    else if(result < B_OK)
    {
      pony_assert(0);
      break;
    }

    bool should_handle_queue = false;
    bool should_quit = false;

    for(int32 i = 0; i < wait_count; i++)
    {
      object_wait_info* info = &(b->wait_list[i]);

      uint16 events = info->events;
      info->events = b->wait_events[i];

      asio_event_t* ev = b->events[i];

      if(ev == NULL)
      {
        if(info->type == B_OBJECT_TYPE_PORT && info->object == b->port)
        {
          should_handle_queue = ((events & B_EVENT_READ) == B_EVENT_READ) && port_count(b->port) > 0;
          should_quit = (events & B_EVENT_INVALID) == B_EVENT_INVALID;
        }

        if(should_handle_queue || should_quit)
        {
          should_refresh_events = true;
          break;
        }
        else
          continue;
      }

      if(events == 0)
      {
        continue;
      }

      if(ev->flags == ASIO_DESTROYED)
      {
        continue;
      }

      uint32_t flags = 0;
      uint32_t count = 0;

      if(events & B_EVENT_READ)
      {
        flags |= ASIO_READ;
        ev->readable = true;

        if(info->events & B_EVENT_HIGH_PRIORITY_READ)
        {
          // Since this is an ASIO_ONESHOT event, set only high priority read
          // (which is not triggered by Haiku, so we're repurposing that one),
          // to keep it a valid object info without triggering it again
          // until a call to resubscribe and B_EVENT_READ is set.
          info->events = info->events & ~(B_EVENT_READ);
          b->wait_events[i] = info->events;
        }
      }

      if(events & B_EVENT_WRITE)
      {
        flags |= ASIO_WRITE;
        ev->writeable = true;

        if(info->events & B_EVENT_HIGH_PRIORITY_WRITE)
        {
          // Same as above, only for a write.
          info->events = info->events & ~(B_EVENT_WRITE);
          b->wait_events[i] = info->events;
        }
      }

      if((events & (B_EVENT_DISCONNECTED | B_EVENT_INVALID | B_EVENT_ERROR)) > 0)
      {
        if(events & B_EVENT_DISCONNECTED)
        {
          if(!(info->events & B_EVENT_HIGH_PRIORITY_READ) || !ev->readable)
          {
            ev->readable = true;
            flags |= ASIO_READ;
          }
        }
        if(events & B_EVENT_ERROR)
        {
          flags |= ASIO_ERROR;
        }
        if(events & B_EVENT_INVALID)
        {
          flags |= ASIO_ERROR;
        }
      }

      // If we had a valid event of some type...
      if(flags != 0)
      {
        // ...send it to the actor.
        pony_asio_event_send(ev, flags, count);
      }
    }

    if(should_handle_queue)
    {
      wfo_handle_queue(b);
    }

    if(should_quit) break;
  }

  wfo_handle_queue(b);

  delete_port(b->port);
  b->port = B_BAD_PORT_ID;

  POOL_FREE(asio_backend_t, b);

  SYSTEMATIC_TESTING_STOP_THREAD();
  TRACING_THREAD_STOP();

  pony_unregister_thread();
  return NULL;
}

static bool wfo_timer_set_nsec(asio_event_t* ev)
{
  struct itimerspec ts;

  pony_assert(ev != NULL);

  uint64_t nsec = ev->nsec;
  timer_t timerID = ev->timerID;

  ts.it_interval.tv_sec = 0;
  ts.it_interval.tv_nsec = 0;
  ts.it_value.tv_sec = (time_t)(nsec / 1000000000);
  ts.it_value.tv_nsec = (long)(nsec - (ts.it_value.tv_sec * 1000000000));

  return timer_settime(timerID, 0, &ts, NULL) == B_OK;
}

static bool wfo_remove_timer(asio_event_t* ev)
{
  if (ev->timerID != NULL)
  {
    timer_delete(ev->timerID);
    ev->timerID = NULL;
  }
  return true;
}

static bool wfo_set_timer(asio_event_t* ev)
{
  struct sigevent event;
  event.sigev_notify = SIGEV_THREAD;
  event.sigev_value.sival_ptr = (void*)ev;
  event.sigev_notify_function = wfo_timer_handler;
  event.sigev_notify_attributes = NULL;

  if(timer_create(CLOCK_MONOTONIC, &event, &ev->timerID) != B_OK)
  {
    goto failure;
  }

  if(!wfo_timer_set_nsec(ev))
  {
    goto failure;
  }

//success:
    return true;

  failure:
    wfo_remove_timer(ev);
    return false;
}

static bool wfo_remove_signal(asio_event_t* ev, int sig)
{
  struct sigaction new_action;

  (void)ev;

#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
  // Make sure we ignore signals related to scheduler sleeping/waking
  // as the default for those signals is termination.
  if(sig == PONY_SCHED_SLEEP_WAKE_SIGNAL)
  {
    new_action.sa_handler = wfo_empty_signal_handler;
    new_action.sa_userdata = NULL;
  }
  else
#endif
    new_action.sa_handler = SIG_DFL;

  sigemptyset (&new_action.sa_mask);

  // Ask to restart interrupted syscalls to match `signal` behavior.
  new_action.sa_flags = SA_RESTART;

  sigaction(sig, &new_action, NULL);

  return true;
}

static bool wfo_set_signal(asio_event_t* ev, int sig)
{
  struct sigaction new_action;
  new_action.sa_handler = (__sighandler_t)(void*)wfo_signal_handler;
  new_action.sa_userdata = (void*)ev;
  sigemptyset (&new_action.sa_mask);

  // Ask to restart interrupted syscalls to match `signal` behavior.
  new_action.sa_flags = SA_RESTART;

  sigaction(sig, &new_action, NULL);

  return true;
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
    // Tell scheduler threads that asio has at least one noisy actor
    // if the old_count was 0.
    if (old_count == 0)
      ponyint_sched_noisy_asio(pony_scheduler_index());
  }

  if(ev->flags & ASIO_TIMER)
  {
    if (!wfo_set_timer(ev))
    {
      pony_asio_event_send(ev, ASIO_ERROR, 0);
      return;
    }
    return;
  }

  if(ev->flags & ASIO_SIGNAL)
  {
    int sig = (int)ev->nsec;
    asio_event_t* prev = NULL;

    // TODO: somehow warn about this?
    if(sig == SIGKILL || sig == SIGSTOP)
    {
      // KILL and STOP are not catchable.
    }

    if((sig < MAX_SIGNAL) &&
      atomic_compare_exchange_strong_explicit(&b->sighandlers[sig], &prev, NULL,
      memory_order_acq_rel, memory_order_acquire))
    {
      if (!wfo_set_signal(ev, sig))
      {
        pony_asio_event_send(ev, ASIO_ERROR, 0);
        return;
      }
    } else {
      return;
    }
  }

  if(ev->flags & (ASIO_READ | ASIO_WRITE))
  {
    if(wfo_send_request(ev, WFO_SUBSCRIBE) < B_OK)
    {
      pony_asio_event_send(ev, ASIO_ERROR, 0);
    }
  }
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
    if(!wfo_timer_set_nsec(ev))
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

  if(ev->flags & ASIO_TIMER)
  {
    wfo_remove_timer(ev);
  }

  if(ev->flags & ASIO_SIGNAL)
  {
    int sig = (int)ev->nsec;
    asio_event_t* prev = ev;

    if((sig < MAX_SIGNAL) &&
      atomic_compare_exchange_strong_explicit(&b->sighandlers[sig], &prev, NULL,
      memory_order_acq_rel, memory_order_acquire))
    {
      wfo_remove_signal(ev, sig);
    }
  }

  if(ev->flags & (ASIO_READ | ASIO_WRITE))
  {
    // Don't send ASIO_ERROR on failure — the actor is tearing down anyway.
    wfo_send_request(ev, WFO_UNSUBSCRIBE);
  }

  // Don't send ASIO_ERROR on failure — the actor is tearing down anyway.
  ev->flags = ASIO_DISPOSABLE;
  wfo_send_request(ev, ASIO_DISPOSABLE);
}

#endif
