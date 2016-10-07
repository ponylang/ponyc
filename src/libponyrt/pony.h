#ifndef pony_pony_h
#define pony_pony_h

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#if defined(__cplusplus)
#  include <atomic>
#  define ATOMIC_TYPE(T) std::atomic<T>
extern "C" {
#else
#  define ATOMIC_TYPE(T) T _Atomic
#endif

#if defined(_MSC_VER)
#  define ATTRIBUTE_MALLOC(f) __declspec(restrict) f
#else
#  define ATTRIBUTE_MALLOC(f) f __attribute__((malloc))
#endif

/** Opaque definition of an actor.
 *
 * The internals of an actor aren't visible to the programmer.
 */
typedef struct pony_actor_t pony_actor_t;

/** Opaque definition of a context.
 *
 * The internals of a context aren't visible to the programmer.
 */
typedef struct pony_ctx_t pony_ctx_t;

/** Message header.
 *
 * This must be the first field in any message structure. The ID is used for
 * dispatch. The index is a pool allocator index and is used for freeing the
 * message. The next pointer should not be read or set.
 */
typedef struct pony_msg_t
{
  uint32_t index;
  uint32_t id;
  ATOMIC_TYPE(struct pony_msg_t*) next;
} pony_msg_t;

/// Convenience message for sending an integer.
typedef struct pony_msgi_t
{
  pony_msg_t msg;
  intptr_t i;
} pony_msgi_t;

/// Convenience message for sending a pointer.
typedef struct pony_msgp_t
{
  pony_msg_t msg;
  void* p;
} pony_msgp_t;

/** Trace function.
 *
 * Each type supplies a trace function. It is invoked with the currently
 * executing context and the object being traced.
 */
typedef void (*pony_trace_fn)(pony_ctx_t* ctx, void* p);

/** Serialise function.
 *
 * Each type may supply a serialise function. It is invoked with the currently
 * executing context, the object being serialised, and an address to serialise
 * to.
 */
typedef void (*pony_serialise_fn)(pony_ctx_t* ctx, void* p, void* addr,
  size_t offset, int m);

/** Dispatch function.
 *
 * Each actor has a dispatch function that is invoked when the actor handles
 * a message.
 */
typedef void (*pony_dispatch_fn)(pony_ctx_t* ctx, pony_actor_t* actor,
  pony_msg_t* m);

/** Finalizer.
 *
 * An actor or object can supply a finalizer, which is called before it is
 * collected.
 */
typedef void (*pony_final_fn)(void* p);

/// Describes a type to the runtime.
typedef const struct _pony_type_t
{
  uint32_t id;
  uint32_t size;
  uint32_t trait_count;
  uint32_t field_count;
  uint32_t field_offset;
  void* instance;
  pony_trace_fn trace;
  pony_trace_fn serialise_trace;
  pony_serialise_fn serialise;
  pony_trace_fn deserialise;
  pony_dispatch_fn dispatch;
  pony_final_fn final;
  uint32_t event_notify;
  uint32_t** traits;
  void* fields;
  void* vtable;
} pony_type_t;

/** Padding for actor types.
 *
 * 56 bytes: initial header, not including the type descriptor
 * 52/104 bytes: heap
 * 44/80 bytes: gc
 */
#if INTPTR_MAX == INT64_MAX
#  define PONY_ACTOR_PAD_SIZE 240
#elif INTPTR_MAX == INT32_MAX
#  define PONY_ACTOR_PAD_SIZE 156
#endif

typedef struct pony_actor_pad_t
{
  pony_type_t* type;
  char pad[PONY_ACTOR_PAD_SIZE];
} pony_actor_pad_t;

/// The currently executing context.
pony_ctx_t* pony_ctx();

/** Create a new actor.
 *
 * When an actor is created, the type is set. This specifies the trace function
 * for the actor's data, the message type function that indicates what messages
 * and arguments an actor is able to receive, and the dispatch function that
 * handles received messages.
 */
ATTRIBUTE_MALLOC(pony_actor_t* pony_create(pony_ctx_t* ctx,
  pony_type_t* type));

/// Allocates a message and sets up the header. The index is a POOL_INDEX.
pony_msg_t* pony_alloc_msg(uint32_t index, uint32_t id);

/// Allocates a message and sets up the header. The size is in bytes.
pony_msg_t* pony_alloc_msg_size(size_t size, uint32_t id);

/// Sends a message to an actor.
void pony_sendv(pony_ctx_t* ctx, pony_actor_t* to, pony_msg_t* m);

/** Convenience function to send a message with no arguments.
 *
 * The dispatch function receives a pony_msg_t.
 */
void pony_send(pony_ctx_t* ctx, pony_actor_t* to, uint32_t id);

/** Convenience function to send a pointer argument in a message
 *
 * The dispatch function receives a pony_msgp_t.
 */
void pony_sendp(pony_ctx_t* ctx, pony_actor_t* to, uint32_t id, void* p);

/** Convenience function to send an integer argument in a message
 *
 * The dispatch function receives a pony_msgi_t.
 */
void pony_sendi(pony_ctx_t* ctx, pony_actor_t* to, uint32_t id, intptr_t i);

/** Store a continuation.
 *
 * This puts a message at the front of the actor's queue, instead of at the
 * back. This is not concurrency safe: an actor should only push a continuation
 * to itself.
 *
 * Not used in Pony.
 */
void pony_continuation(pony_actor_t* self, pony_msg_t* m);

/** Allocate memory on the current actor's heap.
 *
 * This is garbage collected memory. This can only be done while an actor is
 * handling a message, so that there is a current actor.
 */
ATTRIBUTE_MALLOC(void* pony_alloc(pony_ctx_t* ctx, size_t size));

/// Allocate using a HEAP_INDEX instead of a size in bytes.
ATTRIBUTE_MALLOC(void* pony_alloc_small(pony_ctx_t* ctx, uint32_t sizeclass));

/// Allocate when we know it's larger than HEAP_MAX.
ATTRIBUTE_MALLOC(void* pony_alloc_large(pony_ctx_t* ctx, size_t size));

/** Reallocate memory on the current actor's heap.
 *
 * Take heap memory and expand it. This is a no-op if there's already enough
 * space, otherwise it allocates and copies.
 */
ATTRIBUTE_MALLOC(void* pony_realloc(pony_ctx_t* ctx, void* p, size_t size));

/** Allocate memory with a finaliser.
 *
 * Attach a finaliser that will be run on memory when it is collected. Such
 * memory cannot be safely realloc'd.
 */
ATTRIBUTE_MALLOC(void* pony_alloc_final(pony_ctx_t* ctx, size_t size,
  pony_final_fn final));

/// Trigger GC next time the current actor is scheduled
void pony_triggergc(pony_actor_t* actor);

/** Start gc tracing for sending.
 *
 * Call this before sending a message if it has anything in it that can be
 * GCed. Then trace all the GCable items, then call pony_send_done.
 */
void pony_gc_send(pony_ctx_t* ctx);

/** Start gc tracing for receiving.
 *
 * Call this when receiving a message if it has anything in it that can be
 * GCed. Then trace all the GCable items, then call pony_recv_done.
 */
void pony_gc_recv(pony_ctx_t* ctx);

/** Start gc tracing for acquiring.
 *
 * Call this when acquiring objects. Then trace the objects you want to
 * acquire, then call pony_acquire_done. Acquired objects will not be GCed
 * until released even if they are not reachable from Pony code anymore.
 * Acquiring an object will also acquire all objects reachable from it as well
 * as their respective owners. When adding or removing objects from an acquired
 * object graph, you must acquire anything added and release anything removed.
 * A given object (excluding actors) cannot be acquired more than once in a
 * single pony_gc_acquire/pony_acquire_done round. The same restriction applies
 * to release functions.
 */
void pony_gc_acquire(pony_ctx_t* ctx);

/** Start gc tracing for releasing.
 *
 * Call this when releasing acquired objects. Then trace the objects you want
 * to release, then call pony_release_done. If an object was acquired multiple
 * times, it must be released as many times before being GCed.
 */
void pony_gc_release(pony_ctx_t* ctx);

/** Finish gc tracing for sending.
 *
 * Call this after tracing the GCable contents.
 */
void pony_send_done(pony_ctx_t* ctx);

/** Finish gc tracing for receiving.
 *
 * Call this after tracing the GCable contents.
 */
void pony_recv_done(pony_ctx_t* ctx);

/** Finish gc tracing for acquiring.
 *
 * Call this after tracing objects you want to acquire.
 */
void pony_acquire_done(pony_ctx_t* ctx);

/** Finish gc tracing for releasing.
 *
 * Call this after tracing objects you want to release.
 */
void pony_release_done(pony_ctx_t* ctx);

/** Identifiers for reference capabilities when tracing.
 *
 * At runtime, we need to identify if the object is logically mutable,
 * immutable, or opaque.
 */
enum
{
  PONY_TRACE_MUTABLE = 0,
  PONY_TRACE_IMMUTABLE = 1,
  PONY_TRACE_OPAQUE = 2
};

/** Trace memory
 *
 * Call this on allocated blocks of memory that do not have object headers.
 */
void pony_trace(pony_ctx_t* ctx, void* p);

/** Trace an object.
 *
 * This should be called for every pointer field in an object when the object's
 * trace function is invoked.
 *
 * @param ctx The current context.
 * @param p The pointer being traced.
 * @param t The pony_type_t for the object pointed to.
 * @param m Logical mutability of the object pointed to.
 */
void pony_traceknown(pony_ctx_t* ctx, void* p, pony_type_t* t, int m);

/** Trace unknown.
 *
 * This should be called for fields in an object with an unknown type.
 *
 * @param ctx The current context.
 * @param p The pointer being traced.
 * @param m Logical mutability of the object pointed to.
 */
void pony_traceunknown(pony_ctx_t* ctx, void* p, int m);

/** Initialize the runtime.
 *
 * Call this first. It will strip out command line arguments that you should
 * ignore and will return the remaining argc. Then create an actor and send it
 * a message, so that some initial work exists. Use pony_become() if you need
 * to send messages that require allocation and tracing.
 *
 * Then call pony_start().
 *
 * It is not safe to call this again before the runtime has terminated.
 */
int pony_init(int argc, char** argv);

/** Starts the pony runtime.
 *
 * Returns -1 if the scheduler couldn't start, otherwise returns the exit code
 * set with pony_exitcode(), defaulting to 0.
 *
 * If library is false, this call will return when the pony program has
 * terminated. If library is true, this call will return immediately, with an
 * exit code of 0, and the runtime won't terminate until pony_stop() is
 * called. This allows further processing to be done on the current thread.
 *
 * It is not safe to call this again before the runtime has terminated.
 */
int pony_start(bool library);

/**
 * Call this to create a pony_ctx_t for a non-scheduler thread. This has to be
 * done before calling pony_ctx(), and before calling any Pony code from the
 * thread.
 *
 * The thread that calls pony_start() is automatically registered. It's safe,
 * but not necessary, to call this more than once.
 */
void pony_register_thread();

/** Signals that the pony runtime may terminate.
 *
 * This only needs to be called if pony_start() was called with library set to
 * true. This returns the exit code, defaulting to zero. This call won't return
 * until the runtime actually terminates.
 */
int pony_stop();

/** Set the exit code.
 *
 * The value returned by pony_start() will be 0 unless set to something else
 * with this call. If called more than once, the value from the last call is
 * returned.
 */
void pony_exitcode(int code);

/**
 * If an actor is currently unscheduled, this will reschedule it. This is not
 * concurrency safe: only a single actor should reschedule another actor, and
 * it should be sure the target actor is actually unscheduled.
 */
void pony_schedule(pony_ctx_t* ctx, pony_actor_t* actor);

/**
 * The actor will no longer be scheduled. It will not handle messages on its
 * queue until it is rescheduled, or polled on a context. This is not
 * concurrency safe: this should be done on the current actor or an actor that
 * has never been sent a message.
 */
void pony_unschedule(pony_ctx_t* ctx, pony_actor_t* actor);

/**
 * Call this to "become" an actor on a non-scheduler context, i.e. from outside
 * the pony runtime. Following this, pony API calls can be made as if the actor
 * in question were the current actor, eg. pony_alloc, pony_send, pony_create,
 * etc. This should only be called with an unscheduled actor.
 *
 * This can be called with NULL to make no actor the "current" actor for a
 * thread.
 */
void pony_become(pony_ctx_t* ctx, pony_actor_t* actor);

/**
 * Call this to handle an application message on an actor. This will do two
 * things: first, it will possibly gc, and second it will possibly handle a
 * pending application message.
 *
 * A thread must pony_become an actor before it can pony_poll.
 */
void pony_poll(pony_ctx_t* ctx);

#if defined(__cplusplus)
}
#endif

#endif
