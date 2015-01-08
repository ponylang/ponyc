#ifndef asio_event_h
#define asio_event_h

#include <pony.h>
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
  uintptr_t data;       /* file descriptor or other data */
  pony_actor_t* owner;  /* owning actor */
  uint32_t msg_id;      /* I/O handler (actor message) */
  uint32_t flags;       /* event filter flags */
  bool noisy;           /* prevents termination? */
} asio_event_t;

/// Message that carries an event and event flags.
typedef struct asio_msg_t
{
  pony_msg_t msg;
  asio_event_t* event;
  uint32_t flags;
} asio_msg_t;

/** Create a new event.
 *
 *  An event is noisy, if it should prevent the runtime system from terminating
 *  based on quiescence.
 */
asio_event_t* asio_event_create(pony_actor_t* owner, uintptr_t data,
  uint32_t flags, bool noisy);

/** Deallocates an ASIO event.
 */
void asio_event_destroy(asio_event_t* ev);

uintptr_t asio_event_data(asio_event_t* ev);

/// Send a triggered event.
void asio_event_send(asio_event_t* ev, uint32_t flags);

/* Subscribe and unsubscribe are implemented in the corresponding I/O
 * mechanism. Files epoll.c, kqueue.c, ...
 */

/** Subscribe an event for notifications.
 *
 *  Subscriptions are not incremental. Registering an event multiple times will
 *  overwrite the previous event filter mask.
 */
void asio_event_subscribe(asio_event_t* ev);

/** Update an event.
 *
 * Used for timers.
 */
void asio_event_update(asio_event_t* ev, uintptr_t data);

/** Unsubscribe an event.
 *
 *  After a call to unsubscribe, the caller will not receive any further event
 *  notifications for I/O events on the corresponding resource.
 */
void asio_event_subscribe(asio_event_t* ev);

PONY_EXTERN_C_END

#endif
