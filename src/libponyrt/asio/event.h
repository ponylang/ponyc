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
  struct asio_event_t* magic;
  pony_actor_t* owner;  /* owning actor */
  uint32_t msg_id;      /* I/O handler (actor message) */
  int fd;               /* file descriptor */
  uint32_t flags;       /* event filter flags */
  bool noisy;           /* prevents termination? */
  uint64_t nsec;        /* nanoseconds for timers */

#ifdef PLATFORM_IS_LINUX
  // automagically resubscribe to an event on an FD when another event is
  // on the same FD is triggered
  // This is only needed on linux where an event firing on an FD that has
  // ONESHOT enabled will clear all events for the FD and not only the
  // event that was fired.
  bool auto_resub;      /* automagically resubscribe? */
#endif

  bool readable;        /* is fd readable? */
  bool writeable;       /* is fd writeable? */
#ifdef PLATFORM_IS_WINDOWS
  HANDLE timer;         /* timer handle */
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
asio_event_t* pony_asio_event_create(pony_actor_t* owner, int fd,
  uint32_t flags, uint64_t nsec, bool noisy);

/** Deallocates an ASIO event.
 */
void pony_asio_event_destroy(asio_event_t* ev);

int pony_asio_event_fd(asio_event_t* ev);

uint64_t pony_asio_event_nsec(asio_event_t* ev);

/// Send a triggered event.
void pony_asio_event_send(asio_event_t* ev, uint32_t flags, uint32_t arg);

/* Subscribe and unsubscribe are implemented in the corresponding I/O
 * mechanism. Files epoll.c, kqueue.c, ...
 */

/** Subscribe an event for notifications.
 *
 *  Subscriptions are not incremental. Registering an event multiple times will
 *  overwrite the previous event filter mask.
 */
void pony_asio_event_subscribe(asio_event_t* ev);

/** Update an event.
 *
 * Used for timers.
 */
void pony_asio_event_setnsec(asio_event_t* ev, uint64_t nsec);

/** Unsubscribe an event.
 *
 *  After a call to unsubscribe, the caller will not receive any further event
 *  notifications for I/O events on the corresponding resource.
 */
void pony_asio_event_unsubscribe(asio_event_t* ev);

/** Get whether FD is writeable or not
 */
bool pony_asio_event_get_writeable(asio_event_t* ev);

/** Set whether FD is writeable or not
 */
void pony_asio_event_set_writeable(asio_event_t* ev, bool writeable);

/** Get whether FD is readable or not
 */
bool pony_asio_event_get_readable(asio_event_t* ev);

/** Get whether FD is readable or not
 */
void pony_asio_event_set_readable(asio_event_t* ev, bool readable);

PONY_EXTERN_C_END

#endif
