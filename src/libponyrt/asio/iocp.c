#include "event.h"
#include "asio.h"
#include "../mem/pool.h"

#ifdef ASIO_USE_IOCP

struct asio_backend_t
{
  HANDLE completion_port;
};

asio_backend_t* asio_backend_init()
{
  asio_backend_t* b = POOL_ALLOC(asio_backend_t);
  memset(b, 0, sizeof(asio_backend_t));

  if(CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1) == 0)
  {
    POOL_FREE(asio_backend_t, b);
    return NULL;
  }

  return b;
}

void asio_backend_terminate(asio_backend_t* b)
{

}

DEFINE_THREAD_FN(asio_backend_dispatch,
{
  return NULL;
});

void asio_event_subscribe(asio_event_t* ev)
{
  asio_backend_t* b = asio_get_backend();

  if(ev->noisy)
    asio_noisy_add();

  CreateIoCompletionPort((HANDLE)ev->data, b->completion_port,
    (ULONG_PTR)ev, 0);
}

void asio_event_unsubscribe(asio_event_t* ev)
{

}

#endif
