#include "event.h"
#include "asio.h"
#ifdef ASIO_USE_IOCP

asio_backend_t* asio_backend_init()
{
  return NULL;
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
 
}

void asio_event_unsubscribe(asio_event_t* ev)
{

}

#endif
