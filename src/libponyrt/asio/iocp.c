#include "event.h"
#include "asio.h"

#ifdef ASIO_USE_IOCP

#include "../sched/mpmcq.h"
#include "../mem/pool.h"
#include <string.h>
#include <stdbool.h>
#include <winsock2.h>


// Requests to start and stop timers and to listen to stdin go through a
// request queue so that all operations can be performed in a single thread
// (the asio background thread).

struct asio_backend_t
{
  HANDLE wakeup;
  mpmcq_t q;
  bool stop;
};


enum // Event requests
{
  EVREQ_STDIN_NOTIFY,
  EVREQ_STDIN_RESUME,
  EVREQ_SET_TIMER,
  EVREQ_CANCEL_TIMER
};


typedef struct event_req_t
{
  int req;
  asio_event_t* event;
  HANDLE handle;
  uint64_t value;
} event_req_t;


void CALLBACK timer_fire(void* arg, DWORD timer_low, DWORD timer_high)
{
  // A timer has fired, notify the actor
  asio_event_t* event = (asio_event_t*)arg;

  event->flags = ASIO_TIMER;
  asio_event_send(event, ASIO_TIMER, 0);
}


asio_backend_t* asio_backend_init()
{
  asio_backend_t* b = POOL_ALLOC(asio_backend_t);
  memset(b, 0, sizeof(asio_backend_t));

  mpmcq_init(&b->q);
  b->wakeup = CreateEvent(NULL, FALSE, FALSE, NULL);
  b->stop = false;

  if(b->wakeup == NULL)
  {
    POOL_FREE(asio_backend_t, b);
    return NULL;
  }

  return b;
}


void asio_backend_terminate(asio_backend_t* b)
{
  b->stop = true;
  SetEvent(b->wakeup);
}


DECLARE_THREAD_FN(asio_backend_dispatch)
{
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
        event_req_t* req;

        while((req = (event_req_t*)mpmcq_pop(&b->q)) != NULL)
        {
          switch(req->req)
          {
            case EVREQ_STDIN_NOTIFY:
              // Who to notify about stdin events has changed
              stdin_event = req->event;

              if(stdin_event == NULL) // No-one listening, don't wait on stdin
                handleCount = 1;
              else  // Someone wants stdin, include it in the wait set
                handleCount = 2;

              break;

            case EVREQ_STDIN_RESUME:
              // Console events have been read, we can continue waiting on
              // stdin now
              if(stdin_event != NULL)
                handleCount = 2;

              break;

            case EVREQ_SET_TIMER:
            {
              // Windows timer resolution is 100ns, adjust given time
              int64_t dueTime = -(int64_t)req->value / 100;
              LARGE_INTEGER liDueTime;
              liDueTime.LowPart = (DWORD)(dueTime & 0xFFFFFFFF);
              liDueTime.HighPart = (LONG)(dueTime >> 32);

              SetWaitableTimer(req->handle, &liDueTime, 0, timer_fire,
                (void*)req->event, FALSE);
              break;
            }

            case EVREQ_CANCEL_TIMER:
              CancelWaitableTimer(req->handle);
              CloseHandle(req->handle);

              // Now that we've called cancel no more fire APCs can happen for
              // this timer, so we're safe to send the dispose notify now.
              req->event->flags = ASIO_DISPOSABLE;
              asio_event_send(req->event, ASIO_DISPOSABLE, 0);
              break;

            default:  // Something's gone very wrong if we reach here
              break;
          }

          // We're done with the event request
          POOL_FREE(event_req_t, req);
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
        asio_event_send(stdin_event, ASIO_READ, 0);
        break;

      default:
        break;
    }
  }

  CloseHandle(b->wakeup);
  mpmcq_destroy(&b->q);
  POOL_FREE(asio_backend_t, b);
  return NULL;
}


static void send_request(int req, asio_event_t* event, HANDLE handle,
  uint64_t value)
{
  asio_backend_t* b = asio_get_backend();

  event_req_t* request = POOL_ALLOC(event_req_t);
  request->req = req;
  request->event = event;
  request->handle = handle;
  request->value = value;

  mpmcq_push(&b->q, (void*)request);
  SetEvent(b->wakeup);
}


// Called from stdfd.c to resume waiting on stdin after a read
void iocp_resume_stdin()
{
  send_request(EVREQ_STDIN_RESUME, NULL, NULL, 0);
}


void asio_event_subscribe(asio_event_t* ev)
{
  if((ev == NULL) ||
    (ev->flags == ASIO_DISPOSABLE) ||
    (ev->flags == ASIO_DESTROYED))
    return;

  asio_backend_t* b = asio_get_backend();

  if(ev->noisy)
    asio_noisy_add();

  if(ev->data == 0)
  {
    // Need to subscribe to stdin
    send_request(EVREQ_STDIN_NOTIFY, ev, NULL, 0);
    return;
  }

  if((ev->flags & ASIO_TIMER) != 0)
  {
    // Need to start a timer.
    // We can create it here but not start it. That must be done in the
    // background thread because that's where we want the fire APC to happen.
    // ev->data is initially the time (in nsec) and ends up as the handle.
    HANDLE timer = CreateWaitableTimer(NULL, FALSE, NULL);
    uint64_t nsec = ev->data;
    ev->data = (uintptr_t)timer;

    send_request(EVREQ_SET_TIMER, ev, timer, nsec);
  }
}


void asio_event_update(asio_event_t* ev, uintptr_t data)
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
    send_request(EVREQ_SET_TIMER, ev, (HANDLE)ev->data, data);
  }
}


void asio_event_unsubscribe(asio_event_t* ev)
{
  if((ev == NULL) ||
    (ev->flags == ASIO_DISPOSABLE) ||
    (ev->flags == ASIO_DESTROYED))
    return;

  asio_backend_t* b = asio_get_backend();

  if(ev->noisy)
  {
    asio_noisy_remove();
    ev->noisy = false;
  }

  if(ev->data == 0)
  {
    // Need to unsubscribe from stdin
    send_request(EVREQ_STDIN_NOTIFY, NULL, NULL, 0);
  }

  if((ev->flags & (ASIO_READ | ASIO_WRITE)) != 0)
  {
    ev->flags = ASIO_DISPOSABLE;
    asio_event_send(ev, ASIO_DISPOSABLE, 0);
  }

  if((ev->flags & ASIO_TIMER) != 0)
  {
    // Need to cancel a timer.
    // The timer set goes through our request queue and if we did the cancel
    // here it might overtake the set. So you put the cancel through the queue
    // as well.
    send_request(EVREQ_CANCEL_TIMER, ev, (HANDLE)ev->data, 0);
  }
}


#endif
