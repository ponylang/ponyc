#ifndef asio_event_h
#define asio_event_h

#include "../pony.h"
#include <platform.h>
#include <stdbool.h>
#include <stdint.h>

PONY_EXTERN_C_BEGIN

/** Definiton of an ASIO event.
 *
 *  Used to carry user defined data for event notifications.
 */
typedef struct asio_event_t
{
  struct asio_event_t* magic;
  pony_actor_t* owner;  /* owning actor */
  uint32_t msg_id;      /* I/O handler (actor message) */
  int fd;               /* file descriptor */
  uint32_t flags;       /* event filter flags */
  bool noisy;           /* prevents termination? */
  bool readable;        /* is fd readable? */
  bool writeable;       /* is fd writeable? */
  uint64_t nsec;        /* nanoseconds for timers */

#ifdef PLATFORM_IS_WINDOWS
  HANDLE timer;         /* timer handle */
  /* Socket teardown is deferred under the readiness backend: unsubscribe issues
   * a ProcessSocketNotifications REMOVE and sets this marker. While set, the
   * asio thread drops any in-flight readiness packet for this event, and the
   * fd + event are kept alive until the REMOVE packet is seen (which is when
   * the backend closes the fd and sends ASIO_DISPOSABLE). The marker covers
   * only in-flight packets, not re-arm: subscribe/resubscribe/unsubscribe for
   * a socket event are all issued from the owning actor's thread and so are
   * mutually ordered by the actor model -- a re-arm can never race the
   * REMOVE-issuing unsubscribe. A future change that moved re-arm off the actor
   * thread would break that and must revisit this marker. */
  bool removing;
#endif
} asio_event_t;

/// Message that carries an event and event flags.
typedef struct asio_msg_t
{
  pony_msg_t msg;
  asio_event_t* event;
  uint32_t flags;
  uint32_t arg;
} asio_msg_t;

/** Create a new event.
 *
 *  An event is noisy, if it should prevent the runtime system from terminating
 *  based on quiescence.
 */
PONY_API asio_event_t* pony_asio_event_create(pony_actor_t* owner, int fd,
  uint32_t flags, uint64_t nsec, bool noisy);

/** Deallocates an ASIO event.
 */
PONY_API void pony_asio_event_destroy(asio_event_t* ev);

PONY_API int pony_asio_event_fd(asio_event_t* ev);

PONY_API uint64_t pony_asio_event_nsec(asio_event_t* ev);

/// Send a triggered event.
PONY_API void pony_asio_event_send(asio_event_t* ev, uint32_t flags,
  uint32_t arg);

/* Subscribe and unsubscribe are implemented in the corresponding I/O
 * mechanism. Files epoll.c, kqueue.c, ...
 */

/** Subscribe an event for notifications.
 *
 *  Subscriptions are not incremental. For fd-based events, registering an
 *  event multiple times overwrites the previous event filter mask. Signal
 *  events must be subscribed exactly once: a duplicate subscribe occupies an
 *  additional subscriber slot rather than overwriting.
 */
PONY_API void pony_asio_event_subscribe(asio_event_t* ev);

/** Update an event.
 *
 * Used for timers.
 */
PONY_API void pony_asio_event_setnsec(asio_event_t* ev, uint64_t nsec);

/** Unsubscribe an event.
 *
 *  After a call to unsubscribe, the caller will not receive any further event
 *  notifications for I/O events on the corresponding resource.
 */
PONY_API void pony_asio_event_unsubscribe(asio_event_t* ev);

/** Re-arm a one-shot socket event.
 *
 *  Implemented by each platform's I/O backend. The single `resubscribe` plus
 *  the two direction-specific variants form the cross-backend ABI the stdlib
 *  FFI-uses (`packages/net`). Declared here so they get C linkage on every
 *  platform -- without this, a backend compiled as C++ (the Windows build)
 *  would emit C++-mangled names that the FFI references can't resolve.
 */
PONY_API void pony_asio_event_resubscribe(asio_event_t* ev);
PONY_API void pony_asio_event_resubscribe_read(asio_event_t* ev);
PONY_API void pony_asio_event_resubscribe_write(asio_event_t* ev);

/** Get whether the event id disposable or not
 */
PONY_API bool pony_asio_event_get_disposable(asio_event_t* ev);

/** Get whether FD is writeable or not
 */
PONY_API bool pony_asio_event_get_writeable(asio_event_t* ev);

/** Set whether FD is writeable or not
 */
PONY_API void pony_asio_event_set_writeable(asio_event_t* ev, bool writeable);

/** Get whether FD is readable or not
 */
PONY_API bool pony_asio_event_get_readable(asio_event_t* ev);

/** Get whether FD is readable or not
 */
PONY_API void pony_asio_event_set_readable(asio_event_t* ev, bool readable);

PONY_EXTERN_C_END

#endif
