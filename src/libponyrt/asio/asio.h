#ifndef asio_asio_h
#define asio_asio_h

#include <pony.h>
#include <platform.h>
#include <stdbool.h>

#if defined(PLATFORM_IS_LINUX)
#  define ASIO_USE_EPOLL
#elif defined(PLATFORM_IS_MACOSX)
#  define ASIO_USE_KQUEUE
#elif defined(PLATFORM_IS_WINDOWS)
#  define ASIO_USE_IOCP
#else
#  error PLATFORM NOT SUPPORTED!
#endif

#define MAX_EVENTS 64

PONY_EXTERN_C_BEGIN

enum
{
  ASIO_READ  = 1 << 0,
  ASIO_WRITE = 1 << 1,
  ASIO_TIMER = 1 << 2
};

/** Opaque definition of a backend.
 *
 *  An ASIO backend is wrapper type for I/O notification mechanisms available on
 *  a specific platform (e.g. I/O Completion-Ports, kqueue, epoll, select, poll,
 *  ...).
 */
typedef struct asio_backend_t asio_backend_t;

/** Opaque definition of an ASIO base.
 *
 *  A base is a representation of the running instance of some I/O notifcation
 *  mechanism.
 */
typedef struct asio_base_t asio_base_t;

/** Initializes an ASIO backend.
 *
 */
asio_backend_t* asio_backend_init();

/** Returns the underlying mechanism for I/O event notification of the running
 *  ASIO base.
 *
 *  The concrete mechanism is platform specific:
 *    Linux: epoll
 *    MacOSX/BSD: kqueue
 *    Windows: I/O completion ports - to be implemented.
 *
 *  If there is no current running backend, one will be started.
 */
asio_backend_t* asio_get_backend();

/** Attempts to stop an asynchronous event mechanism.
 *
 *  Stopping an event mechanism is only possible if there are no pending "noisy"
 *  subscriptions.
 *
 *  Subscribers need to release their interest in an I/O resource before we can
 *  shut down.
 */
bool asio_stop();

/** Add a noisy event subscription.
 */
void asio_noisy_add();

/** Remove a noisy event subscription.
 */
void asio_noisy_remove();

/** Destroys an ASIO backend.
 *
 *  I/O resource descriptors are not closed upon destruction of a backend.
 *
 *  Destroying a backend causes the dispatch loop to be exited, such that the
 *  ASIO thread can terminate.
 */
void asio_backend_terminate(asio_backend_t* backend);

/** Entry point for the ASIO thread.
 *
 *  Errors are not handled within this function but are delegated to the actor
 *  that has subscribed for an event.
 *
 *  The return code of the ASIO thread is not of interest. Therefore, the ASIO
 *  thread is detached an does not need to be joined.
 */
DECLARE_THREAD_FN(asio_backend_dispatch);

PONY_EXTERN_C_END

#endif
