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

enum ASIO_FLAGS
{
	ASIO_SUCCESS             = 0x0001,
	ASIO_FILT_READ           = 0x0002,
	ASIO_FILT_WRITE          = 0x0004,
	ASIO_READABLE            = 0x0008,
	ASIO_WRITABLE            = 0x0010,
	ASIO_LISTENING           = 0x0020,
	ASIO_PEER_SHUTDOWN       = 0x0040,
	ASIO_CLOSED_UNEXPECTEDLY = 0x0080,
	ASIO_ERROR               = 0x0100,

	ASIO_WOULDBLOCK          = 0x0200
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

/** Attempts to stop an asynchronous event mechanism.
 *
 *  Stopping an event mechanism is only possible if there are no pending "noisy"
 *  subscriptions.
 *
 *  Subscribers need to release their interest in an I/O resource before we can
 *  shut down.
 */
bool asio_stop();

/** Writes a vector of data to a file descriptor.
 *
 *  Any I/O is non-blocking. The requested write operation may therefore return
 *  without having written the entire vector.
 *
 *  Returns the status of the requested operation.
 */
uint32_t asio_writev(intptr_t fd, struct iovec* iov, size_t chunks, size_t* nrp);

/** Reads data from a file descriptor.
 *
 *  A call to readv attempts to read as much data from the file descriptor as
 *  provided within iov.
 *
 *  Returns the status of the requested operation.
 */
uint32_t asio_readv(intptr_t fd, struct iovec* iov, size_t chunks, size_t* nrp);

/** Reads data from a file descriptor.
 *
 *  This is a wrapper for asio_readv for convenience.
 */
uint32_t asio_read(intptr_t fd, void* dest, size_t len, size_t* nrp);

PONY_EXTERN_C_END

#endif
