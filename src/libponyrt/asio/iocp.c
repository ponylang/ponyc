#include "event.h"
#include "asio.h"
#include "../mem/pool.h"

#ifdef ASIO_USE_IOCP

struct asio_backend_t
{
  int dummy;
};

asio_backend_t* asio_backend_init()
{
  asio_backend_t* b = POOL_ALLOC(asio_backend_t);
  memset(b, 0, sizeof(asio_backend_t));
  return b;
}

void asio_backend_terminate(asio_backend_t* b)
{
}

DECLARE_THREAD_FN(asio_backend_dispatch)
{
  asio_backend_t* b = arg;

  POOL_FREE(asio_backend_t, b);
  return NULL;
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

  if(ev->flags & (ASIO_READ | ASIO_WRITE))
  {
    ev->flags = ASIO_DISPOSABLE;
    asio_event_send(ev, ASIO_DISPOSABLE, 0);
  }
}

#endif
