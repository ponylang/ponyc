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

#if defined(USE_SYSTEMATIC_TESTING) && !defined(USE_LOGGER_THREAD)
  #define USE_LOGGER_THREAD
  #include <stdio.h>
#endif

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

const char* const wfo_event_names[] = {
  "ASIO_DISPOSABLE",
  "WFO_WAKEUP",
  "WFO_TIMER_FIRED",
  "WFO_SUBSCRIBE",
  "WFO_RESUBSCRIBE",
  "WFO_UNSUBSCRIBE",
};

struct asio_backend_t
{
  port_id port;

  object_wait_info wait_list[MAX_EVENTS];
  uint16 wait_events[MAX_EVENTS];
  asio_event_t* events[MAX_EVENTS];
  int32 wait_list_count;

  PONY_ATOMIC(asio_event_t*) sighandlers[MAX_SIGNAL];

#if defined(USE_LOGGER_THREAD)
  thread_id logger;
#endif
};

struct wfo_message_t
{
  asio_event_t* ev;
  uint32_t flags;
};

#if defined(USE_LOGGER_THREAD)
#  define LOG_MSG_SIZE 128
#  define LOG(format, ...) if(true){                                     \
    asio_backend_t* b = ponyint_asio_get_backend();                      \
    if(b->logger > 0)                                                    \
    {                                                                    \
      char log_msg[LOG_MSG_SIZE];                                        \
      memset(log_msg, 0, LOG_MSG_SIZE);                                  \
      snprintf(log_msg, LOG_MSG_SIZE, format __VA_OPT__(,) __VA_ARGS__); \
      send_data(b->logger, B_OK, log_msg, LOG_MSG_SIZE);                 \
    }                                                                    \
  }                                                                      \

  static status_t logger(void* data)
  {
    (void)data;
    while(true)
    {
      thread_id sender;
      char msg[LOG_MSG_SIZE];
      int32 code = receive_data(&sender, &msg, LOG_MSG_SIZE);
      if(code == B_SHUTTING_DOWN)
        return B_OK;
      printf("wfo: %d: %s\n", sender, msg);
      fflush(stdout);
    }
    return B_OK;
  }

  static void dump_wait_list(asio_backend_t* b, const char* prefix)
  {
    for(int32 i = 0; i < b->wait_list_count; i++)
    {
      LOG("%s: wait_list[%d] = type %d, id %d, read? %d, write? %d, oneshot? %d",
        prefix,
        i,
        b->wait_list[i].type,
        b->wait_list[i].object,
        (b->wait_list[i].events & B_EVENT_READ) > 0,
        (b->wait_list[i].events & B_EVENT_WRITE) > 0,
        (b->wait_list[i].events & (B_EVENT_HIGH_PRIORITY_READ | B_EVENT_HIGH_PRIORITY_WRITE)) > 0);
    }
  }
#  define LOGLIST(prefix) dump_wait_list(ponyint_asio_get_backend(), prefix)
#else
#  define LOG(...)
#  define LOGLIST(...)
#endif

static void send_request(asio_event_t* ev, int32 req)
{
  asio_backend_t* b = ponyint_asio_get_backend();
  if(b == NULL || b->port < 0)
  {
    LOG("tried to send request %s (req = %d) after backend finalization already happened", wfo_event_names[req], req);
    return;
  }

  struct wfo_message_t msg;
  msg.ev = ev;
  // We store event flags as they are at moment of sending message,
  // becuase handle_queue might be called after ev->flags are changed in meantime.
  msg.flags = ev->flags;

  LOG("sending request %s (req = %d)", wfo_event_names[req], req);
  status_t result = write_port(b->port, req, &msg, sizeof(msg));
  if(result < B_OK)
    LOG("ERROR: sending request failed: %d", result);
  else
    LOG("sending request result: %d", result);
}

static void signal_handler(int sig, void* userData)
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

static void timer_handler(union sigval val)
{
  LOG("timer_handler");
  asio_event_t* ev = (asio_event_t*)val.sival_ptr;

  if(ev == NULL || ev->timerID == NULL)
    return;

  asio_backend_t* b = ponyint_asio_get_backend();
  if(b == NULL || b->port < 0)
    return;

  LOG("handling event for timer %p", ev->timerID);
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
  send_request(ev, WFO_TIMER_FIRED);
  LOG("done event for timer %p", ev->timerID);
}

#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
static void empty_signal_handler(int sig)
{
  (void) sig;
}
#endif

int32 event_add(asio_backend_t* b, asio_event_t* ev, int32 object, uint16 type, uint16 events)
{
  pony_assert(b != NULL);

  int32 index = b->wait_list_count;

  if(index >= (int32)B_COUNT_OF(b->wait_list))
  {
    LOG("ERROR: cannot add any more events");
    return -1;
  }

  if(ev != NULL && ev->wfo_id != -1)
  {
    LOG("ERROR: event already subscribed, unsubscribe it first!");
    return -1;
  }

  if(object < 0)
  {
    LOG("ERROR: Invalid object: %d", object);
    return -1;
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

int32 event_update(asio_backend_t* b, asio_event_t* ev, int32 index, int32 object, uint16 type, uint16 events)
{
  pony_assert(b != NULL);

  if(index >= MAX_EVENTS || index < 0)
    return event_add(b, ev, object, type, events);

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

int32 event_remove(asio_backend_t* b, asio_event_t* ev)
{
  pony_assert(b != NULL);
  pony_assert(ev != NULL);

  int32 index = ev->wfo_id;
  if(index < 0)
    return -1;

  LOG("REMOVE %d, wfo_id = %d", ev->fd, index);
  LOGLIST("before removal");

  ev->wfo_id = -1;

  if(b->events[index] != ev)
  {
    LOG("ERROR: event mismatch");
    return -1;
  }

  int32 last = b->wait_list_count - 1;

  if(b->wait_list_count > 0)
    --b->wait_list_count;

  if(last <= index)
  {
    LOGLIST("after trimming");
    return 0;
  }

  b->wait_list[index] = b->wait_list[last];
  b->wait_events[index] = b->wait_events[last];
  b->events[index] = b->events[last];

  // We could be called after wait_for_objects modified events, before they were refreshed,
  // so refresh them here.
  b->wait_list[index].events = b->wait_events[index];

  asio_event_t* ev2 = b->events[index];
  if(ev2 != NULL && ev2->wfo_id >= 0)
    ev2->wfo_id = index;

  LOGLIST("after removal");

  return 0;
}

static uint16 asio_flags_to_wfo_events(uint32_t flags)
{
  uint16 events = 0;

  // WARNING: This is a hack.
  //          Haiku, AFAIK, does not trigger B_EVENT_PRIORITY_* and B_EVENT_HIGH_PRIORITY_* events.
  //          So we use one of them to keep ONESHOT event valid, when we want to ignore it for now.
  //          Which means that we'll be using either both READ and HIGHT_PRIORITY_READ,
  //          or just the HIGH_PRIORITY_READ.
  //          This allows us to emulate ONESHOT, without a need for dispatch function to
  //          accessing asio_event_t's readable/writeable in a race condition manner,
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

static void handle_queue(asio_backend_t* b)
{
  int32 req;
  struct wfo_message_t msg;
  ssize_t read_size = 0;

  while((read_size = read_port_etc(b->port, &req, &msg, sizeof(msg), B_TIMEOUT, 0)) >= 0)
  {
    LOG("handle_queue got message: %s (req = %d)", wfo_event_names[req], req);
    if(read_size != sizeof(msg))
    {
      // 0 means it was just a wakeup-type message
      if(read_size > 0)
      {
        LOG("ERROR: handle_queue got invalid size of data for a msg: %ld != %ld", read_size, sizeof(msg));
      }
      continue;
    }

    asio_event_t* ev = msg.ev;
    uint32_t flags = msg.flags;
    uint16 events = 0;

    pony_assert(req < (int32)B_COUNT_OF(wfo_event_names));
    LOG("handle_queue message targets event with ev->fd set to %d", ev->fd);

    switch(req)
    {
      case ASIO_DISPOSABLE:
        LOG("DISPOSE %d", ev->fd);
        pony_asio_event_send(ev, ASIO_DISPOSABLE, 0);
        break;

      case WFO_TIMER_FIRED:
        LOG("handling WFO_TIMER_FIRED for timer %p", ev->timerID);
        if(ev->timerID != NULL)
          pony_asio_event_send(ev, ASIO_TIMER, 0);
        LOG("done handling WFO_TIMER_FIRED for timer %p", ev->timerID);
        break;

      case WFO_SUBSCRIBE:
        if(ev->fd < 0) continue;

        events = asio_flags_to_wfo_events(flags);

        LOG("SUBSCRIBE %d for: READ %d, WRITE %d, ONESHOT %d", ev->fd, (flags & ASIO_READ) > 0, (flags & ASIO_WRITE) > 0, (flags & ASIO_ONESHOT) > 1);
        LOG("ev->wfo_id was %d", ev->wfo_id);
        LOG("ev is %p", ev);

        if(ev->wfo_id >= 0 && event_update(b, ev, ev->wfo_id, ev->fd, B_OBJECT_TYPE_FD, events) < B_OK)
          pony_asio_event_send(ev, ASIO_ERROR, 0);
        else if(event_add(b, ev, ev->fd, B_OBJECT_TYPE_FD, events) < B_OK)
          pony_asio_event_send(ev, ASIO_ERROR, 0);
        break;

      case WFO_RESUBSCRIBE:
        if(ev->fd < 0) continue;

        events = asio_flags_to_wfo_events(flags);

        LOG("RESUBSCRIBE %d for: READ %d, WRITE %d, ONESHOT %d", ev->fd, (flags & ASIO_READ) > 0, (flags & ASIO_WRITE) > 0, (flags & ASIO_ONESHOT) > 1);
        LOG("ev->wfo_id was %d", ev->wfo_id);
        LOG("ev is %p", ev);

        if(event_update(b, ev, ev->wfo_id, ev->fd, B_OBJECT_TYPE_FD, events) < B_OK)
          pony_asio_event_send(ev, ASIO_ERROR, 0);
        break;

      case WFO_UNSUBSCRIBE:
        LOG("UNSUBSCRIBE %d", ev->fd);
        // Don't send ASIO_ERROR on delete failure — the actor is tearing down,
        // and ENOENT/EBADF is expected (FD already closed or never registered).
        event_remove(b, ev);
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
  event_add(b, NULL, b->port, B_OBJECT_TYPE_PORT, B_EVENT_READ);

#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
  // Make sure we ignore signals related to scheduler sleeping/waking
  // as the default for those signals is termination.
  struct sigaction new_action;
  new_action.sa_handler = empty_signal_handler;
  new_action.sa_userdata = NULL;
  sigemptyset (&new_action.sa_mask);

  // ask to restart interrupted syscalls to match `signal` behavior
  new_action.sa_flags = SA_RESTART;

  sigaction(PONY_SCHED_SLEEP_WAKE_SIGNAL, &new_action, NULL);
#endif

#if defined(USE_LOGGER_THREAD)
  b->logger = spawn_thread(logger, "pony wfo logger", B_DISPLAY_PRIORITY, NULL);
  resume_thread(b->logger);
#endif

  return b;
}

void ponyint_asio_backend_final(asio_backend_t* b)
{
  delete_port(b->port);
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

  send_request(ev, WFO_RESUBSCRIBE);
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

  rename_thread(get_pthread_thread_id(pthread_self()), "wfo_asio_backend_dispatch");
//  set_thread_priority(get_pthread_thread_id(pthread_self()), B_DISPLAY_PRIORITY);

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

  bool should_refresh_events = false;

  while(true)
  {
    int32 wait_count = b->wait_list_count;

    if(should_refresh_events)
    {
      for(int i = 0; i < wait_count; i++) b->wait_list[i].events = b->wait_events[i];
      should_refresh_events = false;
    }

    LOGLIST("before wait");

#if defined(USE_SYSTEMATIC_TESTING)
    // wait only for 10 milliseconds
    ssize_t result = wait_for_objects_etc(b->wait_list, wait_count, B_RELATIVE_TIMEOUT, 10000);
#else
    // wait indefinitely
    ssize_t result = wait_for_objects(b->wait_list, wait_count);
#endif

    SYSTEMATIC_TESTING_YIELD();
    LOGLIST("results");

    if(result == B_TIMED_OUT || result == B_WOULD_BLOCK)
    {
      ssize_t count = port_count(b->port);
      if(count == B_BAD_PORT_ID)
      {
        // `ponyint_asio_backend_final` simply deletes port, so we know it's time to quit.
        LOG("dispatch: shutdown requested");
        break;
      }
      else if(count < 0)
      {
        LOG("ERROR: dispatch port_count returned %ld", count);
      }
      else if(count > 0)
      {
        LOG("dispatch: timed out, has %ld messages in queue, calling handle_queue...", count);
        handle_queue(b);
      }

      should_refresh_events = true;
      continue;
    }
    else if(result < B_OK)
    {
      LOG("dispatch: ERROR: %ld", result);
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
          LOG("dispatch port update: has messages %d, shutting down %d", should_handle_queue, should_quit);
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
          // Since this is an ASIO_ONESHOT event, set only high priority read (which is not triggered by Haiku),
          // to keep it a valid object info, but not trigger again until a call to resubscribe and B_EVENT_READ is set.
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
          LOG("B_EVENT_DISCONNECTED info->object %d, ev->fd %d", info->object, ev->fd);
          if(!(info->events & B_EVENT_HIGH_PRIORITY_READ) || !ev->readable)
          {
            ev->readable = true;
            flags |= ASIO_READ;
          }
        }
        if(events & B_EVENT_ERROR)
        {
          LOG("B_EVENT_ERROR info->object %d, ev->fd %d", info->object, ev->fd);
        }
        if(events & B_EVENT_INVALID)
        {
          LOG("B_EVENT_INVALID info->object %d, ev->fd %d", info->object, ev->fd);
        }
      }

      // if we had a valid event of some type that needs to be sent
      // to an actor
      if(flags != 0)
      {
        LOG("SEND event for info->object %d, ev->fd %d", info->object, ev->fd);
        // send the event to the actor
        pony_asio_event_send(ev, flags, count);
      }
    }

    if(should_handle_queue)
    {
      LOG("Handling queue...");
      handle_queue(b);
    }

    if(should_quit) break;
  }

  LOG("Shutting down... handling queue for the last time...");
  handle_queue(b);

  delete_port(b->port);
  b->port = B_BAD_PORT_ID;

#if defined(USE_LOGGER_THREAD)
  send_data(b->logger, B_SHUTTING_DOWN, NULL, 0);
  wait_for_thread(b->logger, NULL);
  b->logger = -1;
#endif

  POOL_FREE(asio_backend_t, b);

  SYSTEMATIC_TESTING_STOP_THREAD();
  TRACING_THREAD_STOP();

  pony_unregister_thread();
  return NULL;
}

static bool timer_set_nsec(asio_event_t* ev)
{
  struct itimerspec ts;

  pony_assert(ev != NULL);

  uint64_t nsec = ev->nsec;
  timer_t timerID = ev->timerID;

  ts.it_interval.tv_sec = 0;
  ts.it_interval.tv_nsec = 0;
  ts.it_value.tv_sec = (time_t)(nsec / 1000000000);
  ts.it_value.tv_nsec = (long)(nsec - (ts.it_value.tv_sec * 1000000000));

  LOG("setting timer %p", timerID);

  return timer_settime(timerID, 0, &ts, NULL) == B_OK;
}

static bool wfo_remove_timer(asio_event_t* ev)
{
  if (ev->timerID != NULL)
  {
    timer_delete(ev->timerID);
    LOG("timer %p" " removed", ev->timerID);
  }
  ev->timerID = NULL;
  return true;
}

static bool wfo_set_timer(asio_event_t* ev)
{
  LOG("creating timer");
  struct sigevent event;
  event.sigev_notify = SIGEV_THREAD;
  event.sigev_value.sival_ptr = (void*)ev;
  event.sigev_notify_function = timer_handler;
  event.sigev_notify_attributes = NULL;

  if(timer_create(CLOCK_MONOTONIC, &event, &ev->timerID) != B_OK)
  {
    goto failure;
  }

  LOG("timer %p" " created", ev->timerID);

  if(!timer_set_nsec(ev))
  {
    goto failure;
  }

  LOG("timer %p" " set", ev->timerID);

//success:
    return true;

  failure:
    LOG("ERROR: failed to create timer");
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
    new_action.sa_handler = empty_signal_handler;
    new_action.sa_userdata = NULL;
  }
  else
#endif
    new_action.sa_handler = SIG_DFL;

  sigemptyset (&new_action.sa_mask);

  // ask to restart interrupted syscalls to match `signal` behavior
  new_action.sa_flags = SA_RESTART;

  sigaction(sig, &new_action, NULL);

  return true;
}

static bool wfo_set_signal(asio_event_t* ev, int sig)
{
  struct sigaction new_action;
  new_action.sa_handler = (__sighandler_t)(void*)signal_handler;
  new_action.sa_userdata = (void*)ev;
  sigemptyset (&new_action.sa_mask);

  // ask to restart interrupted syscalls to match `signal` behavior
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
    // tell scheduler threads that asio has at least one noisy actor
    // if the old_count was 0
    if (old_count == 0)
      ponyint_sched_noisy_asio(pony_scheduler_index());
  }

  if(ev->flags & ASIO_TIMER)
  {
    LOG("init timer event");
    if (!wfo_set_timer(ev))
    {
      LOG("failed to init timer event");
      pony_asio_event_send(ev, ASIO_ERROR, 0);
      return;
    }
    LOG("subscribe timer event");
    return;
  }

  if(ev->flags & ASIO_SIGNAL)
  {
    int sig = (int)ev->nsec;
    asio_event_t* prev = NULL;

    // TODO: somehow warn about this?
    if(sig == SIGKILL || sig == SIGSTOP)
    {
      // KILL and STOP are not catchable
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
    send_request(ev, WFO_SUBSCRIBE);
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
    if(!timer_set_nsec(ev))
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
    LOG("removing timer...");
    wfo_remove_timer(ev);
    LOG("... done removing timer");
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
    send_request(ev, WFO_UNSUBSCRIBE);
  }

  ev->flags = ASIO_DISPOSABLE;
  send_request(ev, ASIO_DISPOSABLE);
}

#endif
