#define __STDC_FORMAT_MACROS
#include "cycle.h"
#include "../actor/actor.h"
#include "../sched/scheduler.h"
#include "../ds/stack.h"
#include "../ds/hash.h"
#include "../mem/pool.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#define CYCLE_MIN_DEFERRED (1 << 4)
#define CYCLE_MAX_DEFERRED (1 << 20)
#define CYCLE_CONF_GROUP 32
#define CYCLE_CONF_MASK (CYCLE_CONF_GROUP - 1)

enum
{
  CYCLE_INIT,
  CYCLE_BLOCK,
  CYCLE_UNBLOCK,
  CYCLE_ACK,
  CYCLE_TERMINATE
};

typedef struct block_msg_t
{
  pony_msg_t msg;
  pony_actor_t* actor;
  size_t rc;
  deltamap_t* delta;
} block_msg_t;

typedef struct view_t view_t;
typedef struct perceived_t perceived_t;

typedef struct viewref_t
{
  view_t* view;
  size_t rc;
} viewref_t;

static uint64_t viewref_hash(viewref_t* vref)
{
  return hash_ptr(vref->view);
}

static bool viewref_cmp(viewref_t* a, viewref_t* b)
{
  return a->view == b->view;
}

static void viewref_free(viewref_t* vref)
{
  POOL_FREE(viewref_t, vref);
}

DECLARE_STACK(viewrefstack, viewref_t);
DEFINE_STACK(viewrefstack, viewref_t);

DECLARE_HASHMAP(viewrefmap, viewref_t);
DEFINE_HASHMAP(viewrefmap, viewref_t, viewref_hash, viewref_cmp,
  pool_alloc_size, pool_free_size, viewref_free);

typedef enum
{
  COLOR_BLACK,
  COLOR_GREY,
  COLOR_WHITE
} color_t;

struct view_t
{
  pony_actor_t* actor;
  size_t rc;
  bool blocked;
  bool deferred;
  color_t color;
  viewrefmap_t map;
  deltamap_t* delta;
  perceived_t* perceived;
};

static uint64_t view_hash(view_t* view)
{
  return hash_ptr(view->actor);
}

static bool view_cmp(view_t* a, view_t* b)
{
  return a->actor == b->actor;
}

static void view_free(view_t* view)
{
  viewrefmap_destroy(&view->map);
  POOL_FREE(view_t, view);
}

// no element free for a viewmap. views are kept in many maps.
DECLARE_HASHMAP(viewmap, view_t);
DEFINE_HASHMAP(viewmap, view_t, view_hash, view_cmp, pool_alloc_size,
  pool_free_size, NULL);

struct perceived_t
{
  size_t token;
  size_t ack;
  size_t last_conf;
  viewmap_t map;
};

static uint64_t perceived_hash(perceived_t* per)
{
  return hash_int(per->token);
}

static bool perceived_cmp(perceived_t* a, perceived_t* b)
{
  return a->token == b->token;
}

static void perceived_free(perceived_t* per)
{
  viewmap_destroy(&per->map);
  POOL_FREE(perceived_t, per);
}

DECLARE_HASHMAP(perceivedmap, perceived_t);
DEFINE_HASHMAP(perceivedmap, perceived_t, perceived_hash, perceived_cmp,
  pool_alloc_size, pool_free_size, perceived_free);

typedef struct detector_t
{
  pony_actor_pad_t pad;

  size_t next_token;
  size_t next_deferred;
  size_t since_deferred;

  viewmap_t views;
  viewmap_t deferred;
  viewmap_t finalise;
  perceivedmap_t perceived;

  size_t block_msgs;
  size_t unblock_msgs;
  size_t conf_msgs;
  size_t ack_msgs;
  size_t attempted;
  size_t detected;
  size_t collected;
} detector_t;

static pony_actor_t* cycle_detector;

static view_t* get_view(detector_t* d, pony_actor_t* actor)
{
  view_t key;
  key.actor = actor;

  view_t* view = viewmap_get(&d->views, &key);

  if(view == NULL)
  {
    view = (view_t*)POOL_ALLOC(view_t);
    memset(view, 0, sizeof(view_t));
    view->actor = actor;

    viewmap_put(&d->views, view);

    if(actor_hasfinal(actor))
      viewmap_put(&d->finalise, view);
  }

  return view;
}

static void apply_delta(detector_t* d, view_t* view)
{
  deltamap_t* map = view->delta;

  if(map == NULL)
    return;

  // update refs with delta
  size_t i = HASHMAP_BEGIN;
  delta_t* delta;

  while((delta = deltamap_next(map, &i)) != NULL)
  {
    // if the referenced actor has never blocked, we will insert a view_t
    // that has blocked set to false
    view_t* find = get_view(d, delta_actor(delta));
    size_t rc = delta_rc(delta);

    viewref_t key;
    key.view = find;

    if(rc > 0)
    {
      viewref_t* ref = viewrefmap_get(&view->map, &key);

      if(ref == NULL)
      {
        ref = (viewref_t*)POOL_ALLOC(viewref_t);
        ref->view = find;
        viewrefmap_put(&view->map, ref);
      }

      ref->rc = rc;
    } else {
      viewref_t* ref = viewrefmap_remove(&view->map, &key);

      if(ref != NULL)
        viewref_free(ref);
    }
  }

  deltamap_free(map);
  view->delta = NULL;
}

static bool mark_grey(detector_t* d, view_t* view, size_t rc)
{
  if(!view->blocked)
    return false;

  // apply any stored reference delta
  apply_delta(d, view);

  if(view->deferred)
  {
    viewmap_remove(&d->deferred, view);
    view->deferred = false;
  }

  // if rc > view->rc, this will show as a huge number, which is fine
  // it just means our view is out of date for this actor
  view->rc -= rc;

  if(view->color == COLOR_GREY)
    return false;

  assert(view->color == COLOR_BLACK);
  view->color = COLOR_GREY;
  return true;
}

static void scan_grey(detector_t* d, view_t* view, size_t rc)
{
  viewref_t* ref;
  viewref_t head = {view, rc};
  viewrefstack_t* stack = viewrefstack_push(NULL, &head);

  while(stack != NULL)
  {
    stack = viewrefstack_pop(stack, &ref);

    if(mark_grey(d, ref->view, ref->rc))
    {
      size_t i = HASHMAP_BEGIN;
      viewref_t* child;

      while((child = viewrefmap_next(&ref->view->map, &i)) != NULL)
        stack = viewrefstack_push(stack, child);
    }
  }
}

static bool mark_black(view_t* view, size_t rc, int* count)
{
  if(!view->blocked)
  {
    assert(view->color == COLOR_BLACK);
    return false;
  }

  view->rc += rc;

  if(view->color == COLOR_BLACK)
    return false;

  // keep a count of white nodes colored black
  if(view->color == COLOR_WHITE)
    *count = *count + 1;

  view->color = COLOR_BLACK;
  return true;
}

static int scan_black(view_t* view, size_t rc)
{
  // return the count of white nodes colored black
  int count = 0;
  viewref_t* ref;
  viewref_t head = {view, rc};
  viewrefstack_t* stack = viewrefstack_push(NULL, &head);

  while(stack != NULL)
  {
    stack = viewrefstack_pop(stack, &ref);

    if(mark_black(ref->view, ref->rc, &count))
    {
      size_t i = HASHMAP_BEGIN;
      viewref_t* child;

      while((child = viewrefmap_next(&ref->view->map, &i)) != NULL)
        stack = viewrefstack_push(stack, child);
    }
  }

  return count;
}

static bool mark_white(view_t* view, int* count)
{
  if(view->color != COLOR_GREY)
    return false;

  assert(view->blocked);

  if(view->rc > 0)
  {
    // keep a count of grey nodes colored white minus white nodes colored black
    *count = *count - scan_black(view, 0);
    return false;
  }

  assert(view->perceived == NULL);

  view->color = COLOR_WHITE;
  *count = *count + 1;
  return true;
}

static int scan_white(view_t* view)
{
  // return count of white nodes
  int count = 0;
  viewref_t* ref;
  viewref_t head = {view, 0};
  viewrefstack_t* stack = viewrefstack_push(NULL, &head);

  while(stack != NULL)
  {
    stack = viewrefstack_pop(stack, &ref);

    if(mark_white(ref->view, &count))
    {
      size_t i = HASHMAP_BEGIN;
      viewref_t* child;

      while((child = viewrefmap_next(&ref->view->map, &i)) != NULL)
        stack = viewrefstack_push(stack, child);
    }
  }

  return count;
}

static bool collect_view(perceived_t* per, view_t* view, size_t rc, int* count)
{
  if(view->color == COLOR_WHITE)
  {
    assert(view->deferred == false);
    assert(view->perceived == NULL);

    view->perceived = per;
    viewmap_put(&per->map, view);
  }

  return mark_black(view, rc, count);
}

static int collect_white(perceived_t* per, view_t* view, size_t rc)
{
  // return a count of white nodes colored black
  int count = 0;
  viewref_t* ref;
  viewref_t head = {view, rc};
  viewrefstack_t* stack = viewrefstack_push(NULL, &head);

  while(stack != NULL)
  {
    stack = viewrefstack_pop(stack, &ref);

    if(collect_view(per, ref->view, ref->rc, &count))
    {
      size_t i = HASHMAP_BEGIN;
      viewref_t* child;

      while((child = viewrefmap_next(&ref->view->map, &i)) != NULL)
        stack = viewrefstack_push(stack, child);
    }
  }

  return count;
}

static void send_conf(perceived_t* per)
{
  size_t i = per->last_conf;
  size_t count = 0;
  view_t* view;

  while((view = viewmap_next(&per->map, &i)) != NULL)
  {
    pony_sendi(view->actor, ACTORMSG_CONF, per->token);
    count++;

    if(count == CYCLE_CONF_GROUP)
      break;
  }

  per->last_conf = i;
}

static bool detect(detector_t* d, view_t* view)
{
  assert(view->perceived == NULL);

  scan_grey(d, view, 0);
  int count = scan_white(view);
  assert(count >= 0);

  if(count == 0)
    return false;

  perceived_t* per = (perceived_t*)POOL_ALLOC(perceived_t);
  per->token = d->next_token++;
  per->ack = 0;
  per->last_conf = HASHMAP_BEGIN;
  viewmap_init(&per->map, count);
  perceivedmap_put(&d->perceived, per);

  int count2 = collect_white(per, view, 0);

  (void)count2;
  assert(count2 == count);
  assert(viewmap_size(&per->map) == (size_t)count);

  send_conf(per);
  return true;
}

static void deferred(detector_t* d)
{
  if(d->since_deferred < d->next_deferred)
    return;

  d->attempted++;
  d->since_deferred = 0;

  size_t i = HASHMAP_BEGIN;
  view_t* view;

  if((view = viewmap_next(&d->deferred, &i)) != NULL)
  {
    assert(view->deferred == true);
    viewmap_removeindex(&d->deferred, i);
    view->deferred = false;

    if(detect(d, view))
    {
      d->detected++;

      if(d->next_deferred > CYCLE_MIN_DEFERRED)
        d->next_deferred >>= 1;

      return;
    }
  }

  if(d->next_deferred < CYCLE_MAX_DEFERRED)
    d->next_deferred <<= 1;
}

static void expire(detector_t* d, view_t* view)
{
  perceived_t* per = view->perceived;

  if(per == NULL)
    return;

  size_t i = HASHMAP_BEGIN;
  view_t* pview;

  while((pview = viewmap_next(&per->map, &i)) != NULL)
    pview->perceived = NULL;

  perceivedmap_remove(&d->perceived, per);
  perceived_free(per);
  view->perceived = NULL;
}

static void collect(detector_t* d, perceived_t* per)
{
  perceivedmap_remove(&d->perceived, per);

  size_t i = HASHMAP_BEGIN;
  view_t* view;

  // mark actors in the cycle as pending destruction
  while((view = viewmap_next(&per->map, &i)) != NULL)
  {
    assert(view->perceived == per);

    // remove from the deferred set
    if(view->deferred)
      viewmap_remove(&d->deferred, view);

    actor_setpendingdestroy(view->actor);

    // invoke the actor's finalizer
    if(actor_hasfinal(view->actor))
    {
      viewmap_remove(&d->finalise, view);
      actor_final(view->actor);
    }
  }

  // actors being collected that have references to actors that are not in
  // the cycle now send gc_release messages to those actors
  i = HASHMAP_BEGIN;

  while((view = viewmap_next(&per->map, &i)) != NULL)
    actor_sweep(view->actor);

  // destroy the actor and free the view on the actor
  i = HASHMAP_BEGIN;

  while((view = viewmap_next(&per->map, &i)) != NULL)
  {
    actor_destroy(view->actor);
    viewmap_remove(&d->views, view);

    // no other actor has a viewref to this view
    view_free(view);
  }

  // free the perceived cycle
  perceived_free(per);
  d->collected++;
}

static void block(detector_t* d, pony_actor_t* actor, size_t rc,
  deltamap_t* map)
{
  view_t* view = get_view(d, actor);

  // apply any previous delta
  apply_delta(d, view);

  // store the new delta
  view->delta = map;

  // update reference count
  view->rc = rc;

  // record that we're blocked
  view->blocked = true;

  // add to the deferred set
  if(!view->deferred)
  {
    viewmap_put(&d->deferred, view);
    view->deferred = true;
  }

  d->since_deferred++;

  // if we're in a perceived cycle, that cycle is invalid
  expire(d, view);

  // look for cycles
  deferred(d);
}

static void unblock(detector_t* d, pony_actor_t* actor)
{
  view_t key;
  key.actor = actor;

  // we must be in the views, because we must have blocked in order to unblock
  view_t* view = viewmap_get(&d->views, &key);
  assert(view != NULL);

  // record that we're unblocked
  view->blocked = false;

  // remove from the deferred set
  if(view->deferred)
  {
    viewmap_remove(&d->deferred, view);
    view->deferred = false;
  }

  // if we're in a perceived cycle, that cycle is invalid
  expire(d, view);
}

static void ack(detector_t* d, size_t token)
{
  perceived_t key;
  key.token = token;

  perceived_t* per = perceivedmap_get(&d->perceived, &key);

  if(per == NULL)
    return;

  per->ack++;

  if(per->ack == viewmap_size(&per->map))
  {
    collect(d, per);
    return;
  }

  if((per->ack & CYCLE_CONF_MASK) == 0)
    send_conf(per);
}

static void forcecd(detector_t* d)
{
  // add everything to the deferred set
  size_t i = HASHMAP_BEGIN;
  view_t* view;

  while((view = viewmap_next(&d->views, &i)) != NULL)
  {
    viewmap_put(&d->deferred, view);
    view->deferred = true;
  }

  // handle all deferred detections
  while(viewmap_size(&d->deferred) > 0)
  {
    d->next_deferred = 0;
    deferred(d);
  }

  // terminate the scheduler
  if(viewmap_size(&d->views) == 0)
    scheduler_terminate();
}

static void final(pony_actor_t* self)
{
  detector_t* d = (detector_t*)self;
  size_t i = HASHMAP_BEGIN;
  view_t* view;

  // invoke the actor's finalizer. note that system actors and unscheduled
  // actors will not necessarily be finalised.
  while((view = viewmap_next(&d->finalise, &i)) != NULL)
    actor_final(view->actor);

  // terminate the scheduler
  scheduler_terminate();
}

static void cycle_dispatch(pony_actor_t* self, pony_msg_t* msg)
{
  detector_t* d = (detector_t*)self;

  switch(msg->id)
  {
    case CYCLE_INIT:
    {
      d->next_deferred = CYCLE_MIN_DEFERRED;
      break;
    }

    case CYCLE_BLOCK:
    {
      block_msg_t* m = (block_msg_t*)msg;
      d->block_msgs++;
      block(d, m->actor, m->rc, m->delta);
      break;
    }

    case CYCLE_UNBLOCK:
    {
      pony_msgp_t* m = (pony_msgp_t*)msg;
      d->unblock_msgs++;
      unblock(d, (pony_actor_t*)m->p);
      break;
    }

    case CYCLE_ACK:
    {
      pony_msgi_t* m = (pony_msgi_t*)msg;
      d->ack_msgs++;
      ack(d, m->i);
      break;
    }

    case CYCLE_TERMINATE:
    {
      pony_msgi_t* m = (pony_msgi_t*)msg;

      if(m->i != 0)
        forcecd(d);
      else
        final(self);
      break;
    }
  }
}

static pony_type_t cycle_type =
{
  0,
  sizeof(detector_t),
  0,
  0,
  NULL,
  NULL,
  NULL,
  cycle_dispatch,
  NULL,
  0,
  NULL,
  NULL,
  {}
};

void cycle_create()
{
  cycle_detector = pony_create(&cycle_type);
  actor_setsystem(cycle_detector);
  pony_send(cycle_detector, CYCLE_INIT);
}

void cycle_block(pony_actor_t* actor, gc_t* gc)
{
  block_msg_t* m = (block_msg_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(block_msg_t)), CYCLE_BLOCK);
  m->actor = actor;
  m->rc = gc_rc(gc);
  m->delta = gc_delta(gc);

  pony_sendv(cycle_detector, &m->msg);
}

void cycle_unblock(pony_actor_t* actor)
{
  pony_sendp(cycle_detector, CYCLE_UNBLOCK, actor);
}

void cycle_ack(size_t token)
{
  pony_sendi(cycle_detector, CYCLE_ACK, token);
}

void cycle_terminate(bool forcecd)
{
  if(forcecd)
  {
    pony_sendi(cycle_detector, CYCLE_TERMINATE, forcecd);
  }
  else
  {
    pony_become(cycle_detector);
    final(cycle_detector);
  }
}
