#ifndef pony_pony_h
#define pony_pony_h

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <pony/detail/atomics.h>

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(_MSC_VER)
#  define ATTRIBUTE_MALLOC __declspec(restrict)
#  define PONY_API __declspec(dllexport)
#else
#  define ATTRIBUTE_MALLOC __attribute__((malloc))
#  if defined(_WIN32)
#    define PONY_API __attribute__((dllexport))
#  else
#    define PONY_API __attribute__((visibility("default")))
#  endif
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
typedef struct pony_msg_t pony_msg_t;

struct pony_msg_t
{
  uint32_t index;
  uint32_t id;
  PONY_ATOMIC(pony_msg_t*) next;
};

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
 *
 * A trace function must not raise errors.
 */
typedef void (*pony_trace_fn)(pony_ctx_t* ctx, void* p);

/** Dispatch function.
 *
 * Each actor has a dispatch function that is invoked when the actor handles
 * a message.
 *
 * A dispatch function must not raise errors.
 */
typedef void (*pony_dispatch_fn)(pony_ctx_t* ctx, pony_actor_t* actor,
  pony_msg_t* m);

/** Behavior name function.
 *
 * Each actor has a behavior name function that translates a message id for an
 * actor to a string behavior name that will run for that message.
 */
typedef char* (*pony_behavior_name_fn)(uint32_t id);

/** Finaliser.
 *
 * An actor or object can supply a finaliser, which is called before it is
 * collected.
 *
 * A finaliser must not raise errors.
 */
typedef void (*pony_final_fn)(void* p);

/// Describes a type to the runtime.
typedef const struct _pony_type_t
{
  uint32_t id;
  uint32_t size;
  uint32_t field_count;
  uint32_t field_offset;
  void* instance;
  #if defined(USE_RUNTIME_TRACING)
  char* name;
  pony_behavior_name_fn get_behavior_name;
  #endif
  pony_trace_fn trace;
  pony_dispatch_fn dispatch;
  pony_final_fn final;
  uint32_t event_notify;
  bool might_reference_actor;
  uintptr_t** traits;
  void* fields;
  void* vtable;
} pony_type_t;

/** Language feature initialiser.
 *
 * Contains initialisers for the various language features initialised by
 * the pony_start() function.
 */
typedef struct pony_language_features_init_t
{
  /// Network-related initialisers.
  bool init_network;
} pony_language_features_init_t;

/// The currently executing context.
PONY_API pony_ctx_t* pony_ctx();

/** Create a new actor.
 *
 * When an actor is created, the type is set. This specifies the trace function
 * for the actor's data, the message type function that indicates what messages
 * and arguments an actor is able to receive, and the dispatch function that
 * handles received messages.
 */
PONY_API ATTRIBUTE_MALLOC pony_actor_t* pony_create(pony_ctx_t* ctx,
  pony_type_t* type, bool orphaned);

/// Allocate a message and set up the header. The index is a POOL_INDEX.
PONY_API pony_msg_t* pony_alloc_msg(uint32_t index, uint32_t id);

/** Send a chain of messages to an actor.
 *
 * The first and last messages must either be the same message or the two ends
 * of a chain built with calls to pony_chain.
 *
 * has_app_msg should be true if there are application messages in the chain.
 */
PONY_API void pony_sendv(pony_ctx_t* ctx, pony_actor_t* to, pony_msg_t* first,
  pony_msg_t* last, bool has_app_msg);

/** Single producer version of pony_sendv.
 *
 * This is a more efficient version of pony_sendv in the single producer case.
 * This is unsafe to use with multiple producers, only use this function when
 * you know nobody else will try to send a message to the actor at the same
 * time. This includes messages sent by ASIO event notifiers.
 *
 * The first and last messages must either be the same message or the two ends
 * of a chain built with calls to pony_chain.
 *
 * has_app_msg should be true if there are application messages in the chain.
 */
PONY_API void pony_sendv_single(pony_ctx_t* ctx, pony_actor_t* to,
  pony_msg_t* first, pony_msg_t* last, bool has_app_msg);

/** Chain two messages together.
 *
 * Make a link in a chain of messages in preparation for a subsequent call to
 * pony_sendv with the completed chain. prev will appear before next in the
 * delivery order.
 *
 * prev must either be an unchained message or the end of an existing chain.
 * next must either be an unchained message or the start of an existing chain.
 */
PONY_API void pony_chain(pony_msg_t* prev, pony_msg_t* next);

/** Allocate memory on the current actor's heap.
 *
 * This is garbage collected memory. This can only be done while an actor is
 * handling a message, so that there is a current actor.
 */
PONY_API ATTRIBUTE_MALLOC void* pony_alloc(pony_ctx_t* ctx, size_t size);

/// Allocate using a HEAP_INDEX instead of a size in bytes.
PONY_API ATTRIBUTE_MALLOC void* pony_alloc_small(pony_ctx_t* ctx, uint32_t sizeclass);

/// Allocate when we know it's larger than HEAP_MAX.
PONY_API ATTRIBUTE_MALLOC void* pony_alloc_large(pony_ctx_t* ctx, size_t size);

/** Reallocate memory on the current actor's heap.
 *
 * Take heap memory and expand it. This is a no-op if there's already enough
 * space, otherwise it allocates and copies. The old memory must have been
 * allocated via pony_alloc(), pony_alloc_small(), or pony_alloc_large().
 */
PONY_API ATTRIBUTE_MALLOC void* pony_realloc(pony_ctx_t* ctx, void* p, size_t size, size_t copy);

/** Allocate memory with a finaliser.
 *
 * Attach a finaliser that will be run on memory when it is collected. Such
 * memory cannot be safely realloc'd.
 */
PONY_API ATTRIBUTE_MALLOC void* pony_alloc_final(pony_ctx_t* ctx, size_t size);

/// Allocate using a HEAP_INDEX instead of a size in bytes.
PONY_API ATTRIBUTE_MALLOC void* pony_alloc_small_final(pony_ctx_t* ctx,
  uint32_t sizeclass);

/// Allocate when we know it's larger than HEAP_MAX.
PONY_API ATTRIBUTE_MALLOC void* pony_alloc_large_final(pony_ctx_t* ctx, size_t size);

/// Trigger GC next time the current actor is scheduled
PONY_API void pony_triggergc(pony_ctx_t* ctx);

/** Start gc tracing for sending.
 *
 * Call this before sending a message if it has anything in it that can be
 * GCed. Then trace all the GCable items, then call pony_send_done. `to` is the
 * message's destination actor: the send-trace uses it to recognise a self-send
 * (an object forwarded back to the sending actor) and pin such objects against
 * the local GC sweep while they round-trip through the actor's own queue, so
 * weighted reference counting amortises the owner acquire instead of
 * re-borrowing on every round-trip. Pass NULL if there is no meaningful
 * destination; the send is then never treated as a self-send.
 */
PONY_API void pony_gc_send(pony_ctx_t* ctx, pony_actor_t* to);

/** Start gc tracing for receiving.
 *
 * Call this when receiving a message if it has anything in it that can be
 * GCed. Then trace all the GCable items, then call pony_recv_done.
 */
PONY_API void pony_gc_recv(pony_ctx_t* ctx);

/** Finish gc tracing for sending.
 *
 * Call this after tracing the GCable contents.
 */
PONY_API void pony_send_done(pony_ctx_t* ctx);

/** Finish gc tracing for receiving.
 *
 * Call this after tracing the GCable contents.
 */
PONY_API void pony_recv_done(pony_ctx_t* ctx);

/** Continue gc tracing with another message.
 *
 * When sending multiple messages following each other, you can use this
 * function to trace the content of every message in one
 * pony_gc_send/pony_send_done round instead of doing one pair of calls for each
 * message. Call pony_send_next before tracing the content of a new message.
 * Using this function can reduce the amount of gc-specific messages
 * sent. `to` is the new message's destination actor (used for self-send
 * detection, the same as the pony_gc_send argument, and NULL likewise means
 * "no destination, never a self-send"); it takes effect only after the
 * previous message's trace has been finished.
 */
PONY_API void pony_send_next(pony_ctx_t* ctx, pony_actor_t* to);

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
PONY_API void pony_trace(pony_ctx_t* ctx, void* p);

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
PONY_API void pony_traceknown(pony_ctx_t* ctx, void* p, pony_type_t* t, int m);

/** Trace unknown.
 *
 * This should be called for fields in an object with an unknown type.
 *
 * @param ctx The current context.
 * @param p The pointer being traced.
 * @param m Logical mutability of the object pointed to.
 */
PONY_API void pony_traceunknown(pony_ctx_t* ctx, void* p, int m);

/** Initialize the runtime.
 *
 * Call this first. It will strip out command line arguments that you should
 * ignore and will return the remaining argc.
 *
 * Then call pony_start().
 *
 * The runtime can only be initialised and run once per process; it is not safe
 * to call this more than once.
 */
PONY_API int pony_init(int argc, char** argv);

/** Starts the pony runtime.
 *
 * Returns false if the runtime couldn't start, otherwise returns true with
 * the value pointed by exit_code set with pony_exitcode(), defaulting to 0.
 * exit_code can be NULL if you don't care about the exit code.
 *
 * This call will return when the pony program has terminated.
 *
 * language_features specifies which features of the runtime specific to the
 * Pony language, such as network, should be initialised.
 * If language_features is NULL, no feature will be initialised.
 *
 * The runtime can only be run once per process; it is not safe to call this
 * more than once.
 */
PONY_API bool pony_start(int* exit_code,
  const pony_language_features_init_t* language_features);

PONY_API int32_t pony_scheduler_index();

/** Set the exit code.
 *
 * The value returned by pony_start() will be 0 unless set to something else
 * with this call.
 */
PONY_API void pony_exitcode(int code);

/** Get the exit code.
 *
 * Get the value of the last pony_exitcode() call.
 */
PONY_API int pony_get_exitcode();

#if defined(__cplusplus)
}
#endif

#endif
