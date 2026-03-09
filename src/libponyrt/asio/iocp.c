#define PONY_WANT_ATOMIC_DEFS

#include "event.h"
#include "asio.h"

#ifdef ASIO_USE_IOCP

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


// Requests to start and stop timers and to listen to stdin go through a
// request queue so that all operations can be performed in a single thread
// (the asio background thread).

#define MAX_SIGNAL 32
#define MAX_SIGNAL_SUBSCRIBERS 16

typedef struct signal_subscribers_t {
  PONY_ATOMIC(int) registered;  // 0=no, -1=in-progress, 1=yes
  PONY_ATOMIC(asio_event_t*) subscribers[MAX_SIGNAL_SUBSCRIBERS];
} signal_subscribers_t;

struct asio_backend_t
{
  HANDLE wakeup;
  PONY_ATOMIC(bool) stop;
  signal_subscribers_t sighandlers[MAX_SIGNAL];
  PONY_ATOMIC(uint32_t) pending_signals;
  messageq_t q;
};


enum // Event requests
{
  ASIO_STDIN_NOTIFY = 5,
  ASIO_STDIN_RESUME = 6,
  ASIO_SET_TIMER = 7,
  ASIO_CANCEL_TIMER = 8,
  ASIO_CANCEL_SIGNAL = 9
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

  SetEvent(b->wakeup);
}


static void signal_handler(int sig)
{
  if(sig >= MAX_SIGNAL)
    return;

  // Re-register handler (Windows signal is one-shot)
  signal(sig, signal_handler);
  asio_backend_t* b = ponyint_asio_get_backend();
  pony_assert(b != NULL);

  // Set the pending bit and wake the ASIO thread for fan-out.
  // This avoids iterating the subscriber list from signal context,
  // which would race with the ASIO thread modifying the list.
  atomic_fetch_or_explicit(&b->pending_signals, 1u << sig,
    memory_order_release);
  SetEvent(b->wakeup);
}


static void CALLBACK timer_fire(void* arg, DWORD timer_low, DWORD timer_high)
{
  // A timer has fired, notify the actor
  pony_asio_event_send((asio_event_t*)arg, ASIO_TIMER, 0);
}


asio_backend_t* ponyint_asio_backend_init()
{
  asio_backend_t* b = POOL_ALLOC(asio_backend_t);
  memset(b, 0, sizeof(asio_backend_t));
  ponyint_messageq_init(&b->q);

  b->wakeup = CreateEvent(NULL, FALSE, FALSE, NULL);
  b->stop = false;

  if(b->wakeup == NULL)
  {
    POOL_FREE(asio_backend_t, b);
    return NULL;
  }

  return b;
}


void ponyint_asio_backend_final(asio_backend_t* b)
{
  atomic_store_explicit(&b->stop, true, memory_order_relaxed);
  SetEvent(b->wakeup);
}


DECLARE_THREAD_FN(ponyint_asio_backend_dispatch)
{
  ponyint_cpu_affinity(ponyint_asio_get_cpu());
  ponyint_register_asio_thread();
  asio_backend_t* b = (asio_backend_t*)arg;
  pony_assert(b != NULL);
  asio_event_t* stdin_event = NULL;
  HANDLE handles[2];
  handles[0] = b->wakeup;
  handles[1] = GetStdHandle(STD_INPUT_HANDLE);

  // handleCount indicates:
  // 1 => only listen on queue wake event
  // 2 => listen on queue wake event and stdin
  int handleCount = 1;

#if defined(USE_SYSTEMATIC_TESTING)
  // sleep thread until we're ready to start processing
  SYSTEMATIC_TESTING_WAIT_START(ponyint_asio_get_backend_tid(), ponyint_asio_get_backend_sleep_object());
#endif

  while(!atomic_load_explicit(&b->stop, memory_order_acquire))
  {
    switch(WaitForMultipleObjectsEx(handleCount, handles, FALSE, -1, TRUE))
    {
      SYSTEMATIC_TESTING_YIELD();

      case WAIT_OBJECT_0:
      {
        // Process all items on our queue.
        // Since our wake event is not manual, it has already been reset by the
        // time we reach here.
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
            case ASIO_STDIN_NOTIFY:
              // Who to notify about stdin events has changed
              stdin_event = ev;

              if(stdin_event == NULL) // No-one listening, don't wait on stdin
                handleCount = 1;
              else  // Someone wants stdin, include it in the wait set
                handleCount = 2;
              break;

            case ASIO_STDIN_RESUME:
              // Console events have been read, we can continue waiting on
              // stdin now
              if(stdin_event != NULL)
                handleCount = 2;
              break;

            case ASIO_SET_TIMER:
            {
              // Windows timer resolution is 100ns, adjust given time
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

              // Now that we've called cancel no more fire APCs can happen for
              // this timer, so we're safe to send the dispose notify now.
              ev->flags = ASIO_DISPOSABLE;
              pony_asio_event_send(ev, ASIO_DISPOSABLE, 0);
              break;
            }

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
                signal(sig, SIG_DFL);
                atomic_store_explicit(&subs->registered, 0,
                  memory_order_release);
              }

              ev->flags = ASIO_DISPOSABLE;
              pony_asio_event_send(ev, ASIO_DISPOSABLE, 0);
              break;
            }

            default:  // Something's gone very wrong if we reach here
              break;
          }
        }

        // Process pending signals (set by signal_handler via atomic bitmask)
        uint32_t pending = atomic_exchange_explicit(&b->pending_signals, 0,
          memory_order_acquire);
        while(pending != 0)
        {
          int sig = (int)__pony_ctz(pending);
          pending &= ~(1u << sig);
          signal_subscribers_t* subs = &b->sighandlers[sig];
          for(size_t i = 0; i < MAX_SIGNAL_SUBSCRIBERS; i++)
          {
            asio_event_t* sub = atomic_load_explicit(
              &subs->subscribers[i], memory_order_acquire);
            if(sub != NULL)
              pony_asio_event_send(sub, ASIO_SIGNAL, 1);
          }
        }

        break;
      }

      case WAIT_OBJECT_0 + 1:
        // Stdin has input available.
        // Since the console input signal state is level triggered not edge
        // triggered we have to stop waiting on stdin now until the current
        // input is read.
        handleCount = 1;

        // Notify the stdin event listener
        stdin_event->flags = ASIO_READ;
        pony_asio_event_send(stdin_event, ASIO_READ, 0);
        break;

      default:
        break;
    }
  }

  CloseHandle(b->wakeup);
  ponyint_messageq_destroy(&b->q, true);
  POOL_FREE(asio_backend_t, b);

  SYSTEMATIC_TESTING_STOP_THREAD();
  TRACING_THREAD_STOP();

  pony_unregister_thread();
  return NULL;
}


// Called from stdfd.c to resume waiting on stdin after a read
void ponyint_iocp_resume_stdin()
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
    // Need to start a timer.
    // We can create it here but not start it. That must be done in the
    // background thread because that's where we want the fire APC to happen.
    // ev->data is initially the time (in nsec) and ends up as the handle.
    ev->timer = CreateWaitableTimer(NULL, FALSE, NULL);
    send_request(ev, ASIO_SET_TIMER);
  } else if((ev->flags & ASIO_SIGNAL) != 0) {
    int sig = (int)ev->nsec;
    if(sig < MAX_SIGNAL)
    {
      signal_subscribers_t* subs = &b->sighandlers[sig];

      // Install signal handler synchronously on first subscriber
      int expected = 0;
      if(atomic_compare_exchange_strong_explicit(&subs->registered, &expected,
        -1, memory_order_acq_rel, memory_order_acquire))
      {
        signal(sig, signal_handler);
        atomic_store_explicit(&subs->registered, 1, memory_order_release);
      }
      else
      {
        // Another thread is setting up this signal. Spin until setup completes.
        while(atomic_load_explicit(&subs->registered,
          memory_order_acquire) < 0)
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
    }
  } else if(ev->fd == 0) {
    // Need to subscribe to stdin
    send_request(ev, ASIO_STDIN_NOTIFY);
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

  if((ev->flags & ASIO_TIMER) != 0)
  {
    // Need to restart a timer.
    // That must be done in the background thread because that's where we want
    // the fire APC to happen.
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
    // Need to cancel a timer.
    // The timer set goes through our request queue and if we did the cancel
    // here it might overtake the set. So you put the cancel through the queue
    // as well.
    send_request(ev, ASIO_CANCEL_TIMER);
  } else if((ev->flags & ASIO_SIGNAL) != 0) {
    if(ev->nsec < MAX_SIGNAL)
    {
      send_request(ev, ASIO_CANCEL_SIGNAL);
    } else {
      ev->flags = ASIO_DISPOSABLE;
      pony_asio_event_send(ev, ASIO_DISPOSABLE, 0);
    }
  } else if((ev->flags & (ASIO_READ | ASIO_WRITE)) != 0) {
    // Need to unsubscribe from stdin
    if(ev->fd == 0)
      send_request(NULL, ASIO_STDIN_NOTIFY);

    ev->flags = ASIO_DISPOSABLE;
    pony_asio_event_send(ev, ASIO_DISPOSABLE, 0);
  }
}


#endif
