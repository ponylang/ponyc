#ifndef asio_event_h
#define asio_event_h

#include <pony/pony.h>
#include <stdbool.h>
#include <stdint.h>

/** Definiton of an ASIO event.
 *
 *  Used to carry user defined data for event notifications.
 */
typedef struct asio_event_t
{
	intptr_t fd;          /* file descriptor */
	uint32_t eflags;      /* event filter flags */
	pony_actor_t* owner;  /* owning actor */
	uint32_t msg_id;      /* I/O handler (actor message) */

	bool noisy;           /* prevents termination? */
	void* udata;	        /* opaque user data */
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
 *  The owning actor of this event is determined internally. We assume that the
 *  owning actor is always the actor that calls this function.
 *
 *  An event is noisy, if it should prevent the runtime system from terminating
 *  based on quiescence.
 */
asio_event_t* asio_event_create(intptr_t fd, uint32_t eflags, uint32_t msg_id,
	bool noisy, void* udata);

/** Deallocates an ASIO event.
 *
 *  Deallocation does not imply deactivation. The user is responsible to
 *  unsubscribe an event before deallocating it.
 */
void asio_event_dtor(asio_event_t** ev);

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
 *  An event is not deallocated upon unsubscription.
 */
void asio_event_unsubscribe(asio_event_t* ev);

/// Send a triggered event.
void asio_event_send(asio_event_t* ev, uint32_t flags);

#endif
