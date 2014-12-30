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
  uintptr_t fd;
  uint32_t flags;       /* event filter flags */
  pony_actor_t* owner;  /* owning actor */
  uint32_t msg_id;      /* I/O handler (actor message) */

  bool noisy;           /* prevents termination? */
  void* udata;          /* opaque user data */
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
asio_event_t* asio_event_create(pony_actor_t* owner, uint32_t msg_id,
  uintptr_t fd, uint32_t flags, bool noisy, void* udata);

/// Send a triggered event.
void asio_event_send(asio_event_t* ev, uint32_t flags);

/** Deallocates an ASIO event.
 *
 *  Called automatically when unsubscribing.
 */
void asio_event_destroy(asio_event_t* ev);

/** Subscribe and unsubscribe are implemented in the corresponding I/O mechanism
 *  files epoll.c, kqueue.c, ...
 */

/** Subscribe an event for notifications.
 *
 *  Subscriptions are not incremental. Registering an event multiple times will
 *  overwrite the previous event filter mask.
 */
void asio_event_subscribe(asio_event_t* ev);

/** Unsubscribe an event.
 *
 *  After a call to unsubscribe, the caller will not receive any further event
 *  notifications for I/O events on the corresponding resource.
 *
 *  An event is deallocated upon unsubscription.
 */
void asio_event_subscribe(asio_event_t* ev);

PONY_EXTERN_C_END

#endif
