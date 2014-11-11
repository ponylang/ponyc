#ifndef pony_pony_h
#define pony_pony_h

#include <platform.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

PONY_EXTERN_C_BEGIN

/** Opaque definition of an actor.
 *
 * The internals of an actor aren't visible to the programmer.
 */
typedef struct pony_actor_t pony_actor_t;

/** Padding for actor types.
 *
 * Actor types must have this as their first field.
 */
typedef struct pony_actor_pad_t
{
  // 64 bytes: initial header
  // 120 bytes: heap
  // 80 bytes: gc
  // 8 bytes: next
  // 1 byte: flags
  char pad[273];
} pony_actor_pad_t;

/** Message header.
 *
 * This must be the first field in any message structure. The ID is used for
 * dispatch. The index is a pool allocator index and is used for freeing the
 * message. The next pointer should not be read or set.
 */
typedef struct pony_msg_t
{
  uint32_t size;
  uint32_t id;
  struct pony_msg_t* next;
} pony_msg_t;

/// Message type for argc and argv.
typedef struct pony_main_msg_t
{
  pony_msg_t msg;
  int argc;
  char** argv;
} pony_main_msg_t;

/// Convenience message for sending an integer.
typedef struct pony_msgi_t
{
  pony_msg_t msg;
  intptr_t i;
} pony_msgi_t;

/// Convenience message for sending a float.
typedef struct pony_msgd_t
{
  pony_msg_t msg;
  double d;
} pony_msgd_t;

/// Convenience message for sending a pointer.
typedef struct pony_msgp_t
{
  pony_msg_t msg;
  void* p;
} pony_msgp_t;

/** Trace function.
 *
 * Each type supplies a trace function. It is invoked with the object being
 * traced. In this function, pony_trace() should be called for each field in
 * the object.
 */
typedef void (*pony_trace_fn)(void* p);

/** Dispatch function.
 *
 * Each actor has a dispatch function that is invoked when the actor handles
 * a message.
 */
typedef void (*pony_dispatch_fn)(pony_actor_t* actor, pony_msg_t* m);

/** Actor finalizer.
 *
 *  Each actor supplies a finalizer, which is called
 *  before an actor is collected.
 */
typedef void (*pony_final_fn)(void* p);

/// Describes a type to the runtime.
typedef const struct _pony_type_t
{
  uint32_t id; // Will be used in serialisation.
  uint32_t size; // Size in bytes.
  uint32_t trait_count;
  uint32_t tuple_count;
  pony_trace_fn trace;
  pony_trace_fn serialise;
  pony_trace_fn deserialise;
  pony_dispatch_fn dispatch;
  pony_final_fn final;
  // uint32_t** traits;
  // pony_type_t** tuple;
  // void* vtable[0];
} pony_type_t;

/// This function must be supplied by the program, not the runtime.
pony_type_t* pony_lookup_type(uint32_t id);

/** Create a new actor.
 *
 * When an actor is created, the type is set. This specifies the trace function
 * for the actor's data, the message type function that indicates what messages
 * and arguments an actor is able to receive, and the dispatch function that
 * handles received messages.
 */
__pony_spec_malloc__(pony_actor_t* pony_create(pony_type_t* type));

/// Allocates a message and sets up the header.
pony_msg_t* pony_alloc_msg(uint32_t size, uint32_t id);

/// Sends a message to an actor.
void pony_sendv(pony_actor_t* to, pony_msg_t* m);

/** Convenience function to send a message with no arguments.
 *
 * The dispatch function receives a pony_msg_t.
 */
void pony_send(pony_actor_t* to, uint32_t id);

/** Convenience function to send a pointer argument in a message
 *
 * The dispatch function receives a pony_msgp_t.
 */
void pony_sendp(pony_actor_t* to, uint32_t id, void* p);

/** Convenience function to send an integer argument in a message
 *
 * The dispatch function receives a pony_msgi_t.
 */
void pony_sendi(pony_actor_t* to, uint32_t id, intptr_t i);

/** Convenience function to send a floating point argument in a message
 *
 * The dispatch function receives a pony_msgd_t.
 */
void pony_sendd(pony_actor_t* to, uint32_t id, double d);

/** Convenience function to send argc and argv in a message.
 *
 * The dispatch function receives a pony_main_msg_t.
 */
void pony_sendargs(pony_actor_t* to, uint32_t id, int argc, char** argv);

/** Store a continuation.
 *
 * This puts a message at the front of the actor's queue, instead of at the
 * back. This is not concurrency safe: only a single actor should push a
 * continuation to another actor.
 *
 * Not used in pony.
 */
void pony_continuation(pony_actor_t* to, pony_msg_t* m);

/** Allocate memory on the current actor's heap.
 *
 * This is garbage collected memory. This can only be done while an actor is
 * handling a message, so that there is a current actor.
 */
__pony_spec_malloc__(void* pony_alloc(size_t size));

/** Reallocate memory on the current actor's heap.
 *
 * Take heap memory and expand it. This is a no-op if there's already enough
 * space, otherwise it allocates and copies.
 */
__pony_spec_malloc__(void* pony_realloc(void* p, size_t size));

/// Trigger GC next time the current actor is scheduled
void pony_triggergc();

/**
 * If an actor is currently unscheduled, this will reschedule it. This is not
 * concurrency safe: only a single actor should reschedule another actor, and
 * it should be sure the target actor is actually unscheduled.
 */
void pony_schedule(pony_actor_t* actor);

/**
 * The current actor will no longer be scheduled. It will not handle messages on
 * its queue until it is rescheduled.
 */
void pony_unschedule();

/**
 * Call this to "become" an actor on a non-scheduler thread, i.e. from outside
 * the pony runtime. Following this, pony API calls can be made as if the actor
 * in question were the current actor, eg. pony_alloc, pony_send, pony_create,
 * etc.
 *
 * This can be called with NULL to make no actor the "current" actor for a
 * thread.
 */
void pony_become(pony_actor_t* actor);

/**
 * Call this to handle an application message on an actor. This will do two
 * things: first, it will possibly gc, and second it will possibly handle a
 * pending application message. If an application message is handled, it will
 * return true, otherwise false.
 */
bool pony_poll(pony_actor_t* actor);

/** Start gc tracing for sending.
 *
 * Call this before sending a message if it has anything in it that can be GCed.
 * Then trace all the GCable items, then call pony_gc_done.
 */
void pony_gc_send();

/** Start gc tracing for receiving.
 *
 * Call this when receiving a message if it has anything in it that can be GCed.
 * Then trace all the GCable items, then call pony_gc_done.
 */
void pony_gc_recv();

/** Finish gc tracing for sending.
 *
 * Call this after tracing the GCable contents.
 */
void pony_send_done();

/** Finish gc tracing for receiving.
 *
 * Call this after tracing the GCable contents.
 */
void pony_recv_done();

/** Trace memory
 *
 * Call this on allocated memory that contains no pointers to other allocated
 * memory. Also use this to mark tag aliases.
 */
void pony_trace(void* p);

/** Trace an actor
 *
 * This should be called for fields in an object that point to an actor.
 */
void pony_traceactor(pony_actor_t* p);

/** Trace an object.
 *
 * This should be called for every pointer field in an object when the object's
 * trace function is invoked.
 *
 * @param p The pointer being traced.
 * @param f The trace function for the object pointed to.
 */
void pony_traceobject(void* p, pony_trace_fn f);

/** Initialize the runtime.
 *
 * Call this first. It will strip out command line arguments that you should
 * ignore and will return the remaining argc.
 *
 * Then create an actor and send it a message, so that some initial work exists.
 * Use pony_become() if you need to send messages that require allocation and
 * tracing.
 *
 * Then call pony_start().
 *
 * It is not safe to call this again before the runtime has terminated.
 */
int pony_init(int argc, char** argv);

/// Termination mode for the runtime.
typedef enum
{
  PONY_DONT_WAIT = 0,
  PONY_SYNC_WAIT,
  PONY_ASYNC_WAIT
} pony_termination_t;

/** Starts the pony runtime.
 *
 * Returns -1 if the scheduler couldn't start, otherwise returns the exit code
 * set with pony_exitcode(), defaulting to 0.
 *
 * If the termination mode is PONY_DONT_WAIT, then this call will return as
 * soon as there is no pending work in the system.
 *
 * If it is PONY_SYNC_WAIT, this call won't return until pony_stop() has been
 * called from elsewhere.
 *
 * If it is PONY_ASYNC_WAIT, this call returns immediately, and the runtime
 * won't terminate until pony_stop() is called. This allows further processing
 * to be done on the current thread. The other two modes use the current thread
 * as a scheduler thread.
 *
 * It is not safe to call this again before the runtime has terminated.
 */
int pony_start(pony_termination_t termination);

/** Signals that the pony runtime may terminate.
 *
 * This only needs to be called if pony_start() was called with a termination
 * mode other than PONY_DONT_WAIT. This returns the exit code, defaulting to
 * zero. For the PONY_SYNC_WAIT mode, this exit code should be ignored, and the
 * code from pony_start() should be used instead.
 */
int pony_stop();

/** Set the exit code.
 *
 * The value returned by pony_start() will be 0 unless set to something else
 * with this call. If called more than once, the value from the last call is
 * returned.
 */
void pony_exitcode(int code);

PONY_EXTERN_C_END

#endif
