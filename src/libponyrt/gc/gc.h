#ifndef gc_gc_h
#define gc_gc_h

#include "objectmap.h"
#include "actormap.h"
#include "delta.h"
#include "../mem/heap.h"
#include "../ds/stack.h"
#include "../pony.h"
#include <platform.h>

#define GC_INC_MORE 256
#define GC_ACTOR_HEAP_EQUIV 1024

PONY_EXTERN_C_BEGIN

typedef struct gc_t
{
  uint32_t mark; // First object mark reserved for the current epoch.
  uint32_t rc_mark;
  size_t rc;
  // objectmap size is hashmap mem + (entry mem * num entries)
  objectmap_t local;
  actormap_t foreign;
  // deltamap size is hashmap mem + (entry mem * num entries)
  deltamap_t* delta;
} gc_t;

DECLARE_STACK(ponyint_gcstack, gcstack_t, void);

typedef struct gc_visit_t
{
  void* address;
  pony_type_t* type;
} gc_visit_t;

// An open-addressing set of the (address, type) visits a mark recorded.
typedef struct gc_visitset_t
{
  gc_visit_t* slots;
  size_t count;
  size_t capacity;
} gc_visitset_t;

// Forgets the visits a mark recorded, and releases the storage when the mark
// used little of it.
void ponyint_gc_end_visits(pony_ctx_t* ctx);

// Releases the storage for recorded visits.
void ponyint_gc_free_visits(pony_ctx_t* ctx);

void ponyint_gc_sendobject(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability);

void ponyint_gc_recvobject(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability);

void ponyint_gc_markobject(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability);

// Send, receive and mark functions for a type that can share memory with
// other objects (PONY_EMBED_ROUTED).
void ponyint_gc_sendrouted(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability);

void ponyint_gc_recvrouted(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability);

void ponyint_gc_markrouted(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability);

void ponyint_gc_sendactor(pony_ctx_t* ctx, pony_actor_t* actor);

void ponyint_gc_recvactor(pony_ctx_t* ctx, pony_actor_t* actor);

void ponyint_gc_markactor(pony_ctx_t* ctx, pony_actor_t* actor);

void ponyint_gc_createactor(pony_actor_t* current, pony_actor_t* actor);

object_t* ponyint_gc_next_immutable_root(gc_t* gc, size_t* i);

void ponyint_gc_markimmutable(pony_ctx_t* ctx, gc_t* gc);

void ponyint_gc_handlestack(pony_ctx_t* ctx);

void ponyint_gc_discardstack(pony_ctx_t* ctx);

void ponyint_gc_sweep(pony_ctx_t* ctx, gc_t* gc);

void ponyint_gc_sendacquire(pony_ctx_t* ctx);

void ponyint_gc_sendrelease(pony_ctx_t* ctx, gc_t* gc);

bool ponyint_gc_acquire(gc_t* gc, actorref_t* aref);

bool ponyint_gc_release(gc_t* gc, actorref_t* aref);

size_t ponyint_gc_rc(gc_t* gc);

deltamap_t* ponyint_gc_delta(gc_t* gc);

void ponyint_gc_done(gc_t* gc);

void ponyint_gc_destroy(gc_t* gc);

#ifdef USE_RUNTIMESTATS
size_t ponyint_gc_total_mem_size(pony_actor_t* actor, gc_t* gc);

size_t ponyint_gc_total_alloc_size(pony_actor_t* actor, gc_t* gc);
#endif

PONY_EXTERN_C_END

#endif
