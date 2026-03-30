#ifndef asio_event_h
#define asio_event_h

#include "../pony.h"
#include <platform.h>
#include <stdbool.h>
#include <stdint.h>

PONY_EXTERN_C_BEGIN

#ifdef PLATFORM_IS_WINDOWS
/** Shared liveness token for IOCP operations.
 *
 *  IOCP completion callbacks fire on Windows thread pool threads after the
 *  owning actor may have destroyed the event. Each in-flight IOCP operation
 *  holds a pointer to this token. The callback checks the dead flag before
 *  touching the event.
 *
 *  The refcount starts at 1 (the event's own reference) and tracks three
 *  kinds of holders: each IOCP operation adds 1 (released on callback
 *  completion); each message sent via pony_asio_event_send adds 1
 *  (released in handle_message after the behavior dispatch returns);
 *  destroy marks dead and subtracts the event's 1. Whoever decrements
 *  to zero frees both the event and the token.
 */
typedef struct iocp_token_t
{
  PONY_ATOMIC(bool) dead;
  PONY_ATOMIC(uint32_t) refcount;
} iocp_token_t;
#endif

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
  iocp_token_t* iocp_token; /* shared liveness token for IOCP callbacks */
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
 *  Subscriptions are not incremental. Registering an event multiple times will
 *  overwrite the previous event filter mask.
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
