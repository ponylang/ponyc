#ifndef asio_asio_h
#define asio_asio_h

#include "../pony.h"
#include <platform.h>
#include <stdbool.h>

#if defined(PLATFORM_IS_LINUX)
#  define ASIO_USE_EPOLL
#  define PONY_ASIO_SCHEDULER_INDEX PONY_EPOLL_SCHEDULER_INDEX
#elif defined(PLATFORM_IS_MACOSX) || defined(PLATFORM_IS_BSD)
#  define ASIO_USE_KQUEUE
#  define PONY_ASIO_SCHEDULER_INDEX PONY_KQUEUE_SCHEDULER_INDEX
#elif defined(PLATFORM_IS_WINDOWS)
#  define ASIO_USE_IOCP
#  define PONY_ASIO_SCHEDULER_INDEX PONY_IOCP_SCHEDULER_INDEX
#elif defined(PLATFORM_IS_EMSCRIPTEN)
#  define ASIO_USE_EMSCRIPTEN
#  define PONY_ASIO_SCHEDULER_INDEX PONY_EPOLL_SCHEDULER_INDEX
#else
#  error PLATFORM NOT SUPPORTED!
#endif

#define MAX_EVENTS 64

PONY_EXTERN_C_BEGIN

enum
{
  ASIO_DISPOSABLE = 0,
  ASIO_READ  = 1 << 0,
  ASIO_WRITE = 1 << 1,
  ASIO_TIMER = 1 << 2,
  ASIO_SIGNAL = 1 << 3,
  ASIO_ONESHOT = 1 << 8,
  ASIO_DESTROYED = (uint32_t)-1
};

/** Opaque definition of a backend.
 *
 * An ASIO backend is wrapper type for I/O notification mechanisms available on
 * a specific platform (e.g. I/O Completion-Ports, kqueue, epoll, select, poll,
 * ...).
 */
typedef struct asio_backend_t asio_backend_t;

/** Opaque definition of an ASIO base.
 *
 * A base is a representation of the running instance of some I/O notification
 * mechanism.
 */
typedef struct asio_base_t asio_base_t;

/** Initializes an ASIO backend.
 *
 */
asio_backend_t* ponyint_asio_backend_init();

/// Call this when the scheduler is initialised.
void ponyint_asio_init(uint32_t cpu);

/// Call this when the scheduler runs its threads.
bool ponyint_asio_start();

/** Returns the underlying mechanism for I/O event notification of the running
 * ASIO base.
 *
 * The concrete mechanism is platform specific:
 *   Linux: epoll
 *   MacOSX/BSD: kqueue
 *   Windows: I/O completion ports - to be implemented.
 *
 * If there is no current running backend, one will be started.
 */
asio_backend_t* ponyint_asio_get_backend();

/** Returns the thread id assigned for the ASIO thread.
 *
 */
pony_thread_id_t ponyint_asio_get_backend_tid();

#if defined(USE_SYSTEMATIC_TESTING)
/** Returns the sleep object assigned for the ASIO thread.
 *
 */
pony_signal_event_t ponyint_asio_get_backend_sleep_object();
#endif

/** Returns the cpu assigned for the ASIO thread.
 *
 */
uint32_t ponyint_asio_get_cpu();

/** Attempts to stop an asynchronous event mechanism.
 *
 * Stopping an event mechanism is only possible if there are no pending "noisy"
 * subscriptions.
 *
 * Subscribers need to release their interest in an I/O resource before we can
 * shut down.
 */
bool ponyint_asio_stop();

/** Checks if it is safe to stop the asynchronous event mechanism.
 *
 * Stopping an event mechanism is only possible if there are no pending "noisy"
 * subscriptions.
 */
bool ponyint_asio_stoppable();

/** Add a noisy event subscription.
 */
uint64_t ponyint_asio_noisy_add();

/** Remove a noisy event subscription.
 */
uint64_t ponyint_asio_noisy_remove();

/** Destroys an ASIO backend.
 *
 * I/O resource descriptors are not closed upon destruction of a backend.
 *
 * Destroying a backend causes the dispatch loop to be exited, such that the
 * ASIO thread can terminate.
 */
void ponyint_asio_backend_final(asio_backend_t* backend);

/** Entry point for the ASIO thread.
 *
 * Errors are not handled within this function but are delegated to the actor
 * that has subscribed for an event.
 *
 * The return code of the ASIO thread is not of interest. Therefore, the ASIO
 * thread is detached an does not need to be joined.
 */
DECLARE_THREAD_FN(ponyint_asio_backend_dispatch);


#ifdef ASIO_USE_IOCP

// Resume waiting on stdin after a read.
// Should only be called from stdfd.c.
void ponyint_iocp_resume_stdin();

#endif

#ifdef USE_RUNTIMESTATS
/** Get the static memory used by the asio subsystem.
 */
size_t ponyint_asio_static_mem_size();

/** Get the static memory allocated by the asio subsystem.
 */
size_t ponyint_asio_static_alloc_size();
#endif

PONY_EXTERN_C_END

#endif
