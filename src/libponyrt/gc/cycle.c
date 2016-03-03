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

typedef struct init_msg_t
{
  pony_msg_t msg;
  uint32_t min_deferred;
  uint32_t max_deferred;
  uint32_t conf_group;
} init_msg_t;

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

static size_t viewref_hash(viewref_t* vref)
{
  return hash_ptr(vref->view);
}

static bool viewref_cmp(viewref_t* a, viewref_t* b)
{
  return a->view == b->view;
}

static void viewref_free(viewref_t* vref)
{
  // TODO: view_free(vref->view) ?
  POOL_FREE(viewref_t, vref);
}

DECLARE_STACK(viewrefstack, viewref_t);
DEFINE_STACK(viewrefstack, viewref_t);

DECLARE_HASHMAP(viewrefmap, viewref_t);
DEFINE_HASHMAP(viewrefmap, viewref_t, viewref_hash, viewref_cmp,
  pool_alloc_size, pool_free_size, viewref_free);

enum
{
  COLOR_BLACK,
  COLOR_GREY,
  COLOR_WHITE
} color_t;

struct view_t
{
  pony_actor_t* actor;
  size_t rc;
  uint32_t view_rc;
  bool blocked;
  bool deferred;
  uint8_t color;
  viewrefmap_t map;
  deltamap_t* delta;
  perceived_t* perceived;
};

static size_t view_hash(view_t* view)
{
  return hash_ptr(view->actor);
}

static bool view_cmp(view_t* a, view_t* b)
{
  return a->actor == b->actor;
}

static void view_free(view_t* view)
{
  view->view_rc--;

  if(view->view_rc == 0)
  {
    viewrefmap_destroy(&view->map);

    if(view->delta != NULL)
    {
      deltamap_free(view->delta);
      view->delta = NULL;
    }

    POOL_FREE(view_t, view);
  }
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

static size_t perceived_hash(perceived_t* per)
{
  return hash_size(per->token);
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
  size_t min_deferred;
  size_t max_deferred;
  size_t conf_group;
  size_t next_deferred;
  size_t since_deferred;

  viewmap_t views;
  viewmap_t deferred;
  perceivedmap_t perceived;

  size_t attempted;
  size_t detected;
  size_t collected;

  size_t created;
  size_t destroyed;
} detector_t;

static pony_actor_t* cycle_detector;

static view_t* get_view(detector_t* d, pony_actor_t* actor, bool create)
{
  view_t key;
  key.actor = actor;

  view_t* view = viewmap_get(&d->views, &key);

  if((view == NULL) && create)
  {
    view = (view_t*)POOL_ALLOC(view_t);
    memset(view, 0, sizeof(view_t));
    view->actor = actor;
    view->view_rc = 1;

    viewmap_put(&d->views, view);
    d->created++;
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
    // If rc is 0, we skip creating a view for the actor.
    pony_actor_t* actor = delta_actor(delta);
    size_t rc = delta_rc(delta);

    // If the referenced actor has never blocked, we will insert a view_t
    // that has blocked set to false.
    view_t* find = get_view(d, actor, rc > 0);

    if(find == NULL)
      continue;

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
        find->view_rc++;
      }

      ref->rc = rc;
    } else {
      viewref_t* ref = viewrefmap_remove(&view->map, &key);

      if(ref != NULL)
      {
        viewref_free(ref);
        view_free(find);
      }
    }
  }

  deltamap_free(map);
  view->delta = NULL;
}

static bool mark_grey(detector_t* d, view_t* view, size_t rc)
{
  if(!view->blocked || (view->actor == NULL))
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
  if(!view->blocked || (view->actor == NULL))
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

static void send_conf(pony_ctx_t* ctx, detector_t* d, perceived_t* per)
{
  size_t i = per->last_conf;
  size_t count = 0;
  view_t* view;

  while((view = viewmap_next(&per->map, &i)) != NULL)
  {
    pony_sendi(ctx, view->actor, ACTORMSG_CONF, per->token);
    count++;

    if(count == d->conf_group)
      break;
  }

  per->last_conf = i;
}

static bool detect(pony_ctx_t* ctx, detector_t* d, view_t* view)
{
  assert(view->perceived == NULL);

  scan_grey(d, view, 0);
  int count = scan_white(view);
  assert(count >= 0);

  if(count == 0)
    return false;

  d->detected++;

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

  send_conf(ctx, d, per);
  return true;
}

static void deferred(pony_ctx_t* ctx, detector_t* d)
{
  d->since_deferred++;

  if(d->since_deferred < d->next_deferred)
    return;

  d->attempted++;

  bool found = false;
  size_t i = HASHMAP_BEGIN;
  view_t* view;

  while((view = viewmap_next(&d->deferred, &i)) != NULL)
  {
    assert(view->deferred == true);
    viewmap_removeindex(&d->deferred, i);
    view->deferred = false;

    if(!detect(ctx, d, view))
      break;

    found = true;
  }

  if(found)
  {
    if(d->next_deferred > d->min_deferred)
      d->next_deferred >>= 1;
  } else {
    if(d->next_deferred < d->max_deferred)
      d->next_deferred <<= 1;

    d->since_deferred = 0;
  }
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

static void collect(pony_ctx_t* ctx, detector_t* d, perceived_t* per)
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

    // invoke the actor's finalizer
    actor_setpendingdestroy(view->actor);
    actor_final(ctx, view->actor);
  }

  // actors being collected that have references to actors that are not in
  // the cycle now send gc_release messages to those actors
  i = HASHMAP_BEGIN;

  while((view = viewmap_next(&per->map, &i)) != NULL)
    actor_sendrelease(ctx, view->actor);

  // destroy the actor and free the view on the actor
  i = HASHMAP_BEGIN;

  while((view = viewmap_next(&per->map, &i)) != NULL)
  {
    actor_destroy(view->actor);
    viewmap_remove(&d->views, view);
    view_free(view);
  }

  d->destroyed += viewmap_size(&per->map);

  // free the perceived cycle
  perceived_free(per);
  d->collected++;
}

static void block(pony_ctx_t* ctx, detector_t* d, pony_actor_t* actor,
  size_t rc, deltamap_t* map)
{
  view_t* view = get_view(d, actor, true);

  // update reference count
  view->rc = rc;

  // apply any previous delta and store the new delta
  if(map != NULL)
  {
    apply_delta(d, view);
    view->delta = map;
  }

  // record that we're blocked
  view->blocked = true;

  // if we're in a perceived cycle, that cycle is invalid
  expire(d, view);

  if(rc == 0)
  {
    // remove from the deferred set
    if(view->deferred)
    {
      viewmap_remove(&d->deferred, view);
      view->deferred = false;
    }

    // detect from this actor, bypassing deferral
    detect(ctx, d, view);
  } else {
    // add to the deferred set
    if(!view->deferred)
    {
      viewmap_put(&d->deferred, view);
      view->deferred = true;
    }

    // look for cycles
    deferred(ctx, d);
  }
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

static void ack(pony_ctx_t* ctx, detector_t* d, size_t token)
{
  perceived_t key;
  key.token = token;

  perceived_t* per = perceivedmap_get(&d->perceived, &key);

  if(per == NULL)
    return;

  per->ack++;

  if(per->ack == viewmap_size(&per->map))
  {
    collect(ctx, d, per);
    return;
  }

  if((per->ack & (d->conf_group - 1)) == 0)
    send_conf(ctx, d, per);
}

static void final(pony_ctx_t* ctx, pony_actor_t* self)
{
  // Find block messages and invoke finalisers for those actors
  pony_msg_t* msg;

  while((msg = messageq_pop(&self->q)) != NULL)
  {
    if(msg->id == ACTORMSG_BLOCK)
    {
      block_msg_t* m = (block_msg_t*)msg;

      if(m->delta != NULL)
        deltamap_free(m->delta);

      if(!actor_pendingdestroy(m->actor))
      {
        actor_setpendingdestroy(m->actor);
        actor_final(ctx, m->actor);
      }
    }
  }

  detector_t* d = (detector_t*)self;
  size_t i = HASHMAP_BEGIN;
  view_t* view;

  // Invoke the actor's finalizer. Note that system actors and unscheduled
  // actors will not necessarily be finalised. If the actor isn't marked as
  // blocked, it has already been destroyed.
  while((view = viewmap_next(&d->views, &i)) != NULL)
  {
    if(view->blocked && !actor_pendingdestroy(view->actor))
    {
      actor_setpendingdestroy(view->actor);
      actor_final(ctx, view->actor);
    }
  }

  // Terminate the scheduler.
  scheduler_terminate();
}

#ifndef NDEBUG

static void dump_view(view_t* view)
{
  printf("%p: " __zu "/" __zu " (%s)%s%s\n",
    view->actor, view->rc, view->actor->gc.rc,
    view->blocked ? "blocked" : "unblocked",
    view->rc == view->actor->gc.rc ? "" : " ERROR",
    view->actor->gc.delta == NULL ? "" : " DELTA");

  size_t i = HASHMAP_BEGIN;
  viewref_t* p;
  actorref_t* aref;

  while((p = viewrefmap_next(&view->map, &i)) != NULL)
  {
    aref = actormap_getactor(&view->actor->gc.foreign, p->view->actor);

    if(aref != NULL)
    {
      printf("\t%p: " __zu "/" __zu " %s\n",
        p->view->actor, p->rc, aref->rc,
        p->rc == aref->rc ? "" : "ERROR");
    } else {
      printf("\t%p: " __zu " ERROR\n", p->view->actor, p->rc);
    }
  }

  if(actormap_size(&view->actor->gc.foreign) != viewrefmap_size(&view->map))
  {
    printf("\t--- ERROR\n");

    i = HASHMAP_BEGIN;

    while((aref = actormap_next(&view->actor->gc.foreign, &i)) != NULL)
    {
      printf("\t%p: " __zu "\n", aref->actor, aref->rc);
    }
  }
}

void dump_views()
{
  detector_t* d = (detector_t*)cycle_detector;
  size_t i = HASHMAP_BEGIN;
  view_t* view;

  while((view = viewmap_next(&d->views, &i)) != NULL)
  {
    apply_delta(d, view);
    dump_view(view);
  }
}

void check_view(detector_t* d, view_t* view)
{
  if(view->perceived != NULL)
  {
    printf("%p: in a cycle\n", view->actor);
    return;
  }

  scan_grey(d, view, 0);
  int count = scan_white(view);
  assert(count >= 0);

  printf("%p: %s\n", view->actor, count > 0 ? "COLLECTABLE" : "uncollectable");
}

void check_views()
{
  detector_t* d = (detector_t*)cycle_detector;
  size_t i = HASHMAP_BEGIN;
  view_t* view;

  while((view = viewmap_next(&d->views, &i)) != NULL)
  {
    check_view(d, view);
  }
}

#endif

static void cycle_dispatch(pony_ctx_t* ctx, pony_actor_t* self,
  pony_msg_t* msg)
{
  detector_t* d = (detector_t*)self;

  switch(msg->id)
  {
    case ACTORMSG_BLOCK:
    {
      block_msg_t* m = (block_msg_t*)msg;
      block(ctx, d, m->actor, m->rc, m->delta);
      break;
    }

    case ACTORMSG_UNBLOCK:
    {
      pony_msgp_t* m = (pony_msgp_t*)msg;
      unblock(d, (pony_actor_t*)m->p);
      break;
    }

    case ACTORMSG_ACK:
    {
      pony_msgi_t* m = (pony_msgi_t*)msg;
      ack(ctx, d, m->i);
      break;
    }

#ifndef NDEBUG
    default:
    {
      // Never happens, used to keep debug functions.
      dump_views();
      check_views();
    }
#endif
  }
}

static pony_type_t cycle_type =
{
  0,
  sizeof(detector_t),
  0,
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
  NULL
};

void cycle_create(pony_ctx_t* ctx, uint32_t min_deferred,
  uint32_t max_deferred, uint32_t conf_group)
{
  if(min_deferred > 30)
    min_deferred = 30;

  if(max_deferred > 30)
    max_deferred = 30;

  if(max_deferred < min_deferred)
    max_deferred = min_deferred;

  if(conf_group > 30)
    conf_group = 30;

  cycle_detector = pony_create(ctx, &cycle_type);
  actor_setsystem(cycle_detector);

  detector_t* d = (detector_t*)cycle_detector;
  d->min_deferred = (size_t)1 << (size_t)min_deferred;
  d->max_deferred = (size_t)1 << (size_t)max_deferred;
  d->conf_group = (size_t)1 << (size_t)conf_group;
  d->next_deferred = min_deferred;
}

void cycle_block(pony_ctx_t* ctx, pony_actor_t* actor, gc_t* gc)
{
  assert(ctx->current == actor);
  assert(&actor->gc == gc);

  block_msg_t* m = (block_msg_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(block_msg_t)), ACTORMSG_BLOCK);

  m->actor = actor;
  m->rc = gc_rc(gc);
  m->delta = gc_delta(gc);
  assert(gc->delta == NULL);

  pony_sendv(ctx, cycle_detector, &m->msg);
}

void cycle_unblock(pony_ctx_t* ctx, pony_actor_t* actor)
{
  pony_sendp(ctx, cycle_detector, ACTORMSG_UNBLOCK, actor);
}

void cycle_ack(pony_ctx_t* ctx, size_t token)
{
  pony_sendi(ctx, cycle_detector, ACTORMSG_ACK, token);
}

void cycle_terminate(pony_ctx_t* ctx)
{
  pony_become(ctx, cycle_detector);
  final(ctx, cycle_detector);
}

bool is_cycle(pony_actor_t* actor)
{
  return actor == cycle_detector;
}
