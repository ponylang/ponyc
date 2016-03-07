#include "event.h"
#include "asio.h"

#ifdef ASIO_USE_IOCP

#include "../actor/messageq.h"
#include "../mem/pool.h"
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <winsock2.h>


// Requests to start and stop timers and to listen to stdin go through a
// request queue so that all operations can be performed in a single thread
// (the asio background thread).

#define MAX_SIGNAL 32

struct asio_backend_t
{
  HANDLE wakeup;
  bool stop;
  asio_event_t* sighandlers[MAX_SIGNAL];
  messageq_t q;
};


enum // Event requests
{
  ASIO_STDIN_NOTIFY = 5,
  ASIO_STDIN_RESUME = 6,
  ASIO_SET_TIMER = 7,
  ASIO_CANCEL_TIMER = 8,
  ASIO_SET_SIGNAL = 9,
  ASIO_CANCEL_SIGNAL = 10
};


static void send_request(asio_event_t* ev, int req)
{
  asio_backend_t* b = ponyint_asio_get_backend();

  asio_msg_t* msg = (asio_msg_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(asio_msg_t)), 0);
  msg->event = ev;
  msg->flags = req;
  ponyint_messageq_push(&b->q, (pony_msg_t*)msg);

  SetEvent(b->wakeup);
}


static void signal_handler(int sig)
{
  if(sig >= MAX_SIGNAL)
    return;

  // Reset the signal handler.
  signal(sig, signal_handler);
  asio_backend_t* b = ponyint_asio_get_backend();
  asio_event_t* ev = b->sighandlers[sig];
  pony_asio_event_send(ev, ASIO_SIGNAL, 1);
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
  b->stop = true;
  SetEvent(b->wakeup);
}


DECLARE_THREAD_FN(ponyint_asio_backend_dispatch)
{
  pony_register_thread();
  asio_backend_t* b = (asio_backend_t*)arg;
  asio_event_t* stdin_event = NULL;
  HANDLE handles[2];
  handles[0] = b->wakeup;
  handles[1] = GetStdHandle(STD_INPUT_HANDLE);

  // handleCount indicates:
  // 1 => only listen on queue wake event
  // 2 => listen on queue wake event and stdin
  int handleCount = 1;

  while(!b->stop)
  {
    switch(WaitForMultipleObjectsEx(handleCount, handles, FALSE, -1, TRUE))
    {
      case WAIT_OBJECT_0:
      {
        // Process all items on our queue.
        // Since our wake event is not manual, it has already been reset by the
        // time we reach here.
        asio_msg_t* msg;

        while((msg = (asio_msg_t*)ponyint_messageq_pop(&b->q)) != NULL)
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
              asio_event_t* ev = msg->event;
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

            default:  // Something's gone very wrong if we reach here
              break;
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
  ponyint_messageq_destroy(&b->q);
  POOL_FREE(asio_backend_t, b);
  return NULL;
}


// Called from stdfd.c to resume waiting on stdin after a read
void ponyint_iocp_resume_stdin()
{
  send_request(NULL, ASIO_STDIN_RESUME);
}


void pony_asio_event_subscribe(asio_event_t* ev)
{
  if((ev == NULL) ||
    (ev->flags == ASIO_DISPOSABLE) ||
    (ev->flags == ASIO_DESTROYED))
    return;

  asio_backend_t* b = ponyint_asio_get_backend();

  if(ev->noisy)
    ponyint_asio_noisy_add();

  if((ev->flags & ASIO_TIMER) != 0)
  {
    // Need to start a timer.
    // We can create it here but not start it. That must be done in the
    // background thread because that's where we want the fire APC to happen.
    // ev->data is initially the time (in nsec) and ends up as the handle.
    ev->timer = CreateWaitableTimer(NULL, FALSE, NULL);
    send_request(ev, ASIO_SET_TIMER);
  } else if((ev->flags & ASIO_SIGNAL) != 0) {
    if(ev->nsec < MAX_SIGNAL)
      send_request(ev, ASIO_SET_SIGNAL);
  } else if(ev->fd == 0) {
    // Need to subscribe to stdin
    send_request(ev, ASIO_STDIN_NOTIFY);
  }
}


void pony_asio_event_setnsec(asio_event_t* ev, uint64_t nsec)
{
  if((ev == NULL) ||
    (ev->flags == ASIO_DISPOSABLE) ||
    (ev->flags == ASIO_DESTROYED))
    return;

  if((ev->flags & ASIO_TIMER) != 0)
  {
    // Need to restart a timer.
    // That must be done in the background thread because that's where we want
    // the fire APC to happen.
    ev->nsec = nsec;
    send_request(ev, ASIO_SET_TIMER);
  }
}


void pony_asio_event_unsubscribe(asio_event_t* ev)
{
  if((ev == NULL) ||
    (ev->flags == ASIO_DISPOSABLE) ||
    (ev->flags == ASIO_DESTROYED))
    return;

  asio_backend_t* b = ponyint_asio_get_backend();

  if(ev->noisy)
  {
    ponyint_asio_noisy_remove();
    ev->noisy = false;
  }

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
