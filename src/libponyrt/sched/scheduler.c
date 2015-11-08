#include "scheduler.h"
#include "cpu.h"
#include "mpmcq.h"
#include "../actor/actor.h"
#include "../gc/cycle.h"
#include "../asio/asio.h"
#include "../mem/pool.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

static DECLARE_THREAD_FN(run_thread);

typedef enum
{
  SCHED_BLOCK,
  SCHED_UNBLOCK,
  SCHED_CNF,
  SCHED_ACK,
  SCHED_TERMINATE
} sched_msg_t;

// Scheduler global data.
static uint32_t scheduler_count;
static scheduler_t* scheduler;
static bool volatile detect_quiescence;
static bool use_yield;
static mpmcq_t inject;
static __pony_thread_local scheduler_t* this_scheduler;

/**
 * Gets the next actor from the scheduler queue.
 */
static pony_actor_t* pop(scheduler_t* sched)
{
  return (pony_actor_t*)mpmcq_pop(&sched->q);
}

/**
 * Puts an actor on the scheduler queue.
 */
static void push(scheduler_t* sched, pony_actor_t* actor)
{
  mpmcq_push_single(&sched->q, actor);
}

/**
 * Handles the global queue and then pops from the local queue
 */
static pony_actor_t* pop_global(scheduler_t* sched)
{
  pony_actor_t* actor = (pony_actor_t*)mpmcq_pop(&inject);

  if(actor != NULL)
    return actor;

  return pop(sched);
}

/**
 * Sends a message to a thread.
 */
static void send_msg(uint32_t to, sched_msg_t msg, uint64_t arg)
{
  pony_msgi_t* m = (pony_msgi_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(pony_msgi_t)), msg);

  m->i = arg;
  messageq_push(&scheduler[to].mq, &m->msg);
}

static void read_msg(scheduler_t* sched)
{
  pony_msgi_t* m;

  while((m = (pony_msgi_t*)messageq_pop(&sched->mq)) != NULL)
  {
    switch(m->msg.id)
    {
      case SCHED_BLOCK:
      {
        sched->block_count++;

        if(detect_quiescence && (sched->block_count == scheduler_count))
        {
          // If we think all threads are blocked, send CNF(token) to everyone.
          for(uint32_t i = 0; i < scheduler_count; i++)
            send_msg(i, SCHED_CNF, sched->ack_token);
        }
        break;
      }

      case SCHED_UNBLOCK:
      {
        // Cancel all acks and increment the ack token, so that any pending
        // acks in the queue will be dropped when they are received.
        sched->block_count--;
        sched->ack_token++;
        sched->ack_count = 0;
        break;
      }

      case SCHED_CNF:
      {
        // Echo the token back as ACK(token).
        send_msg(0, SCHED_ACK, m->i);
        break;
      }

      case SCHED_ACK:
      {
        // If it's the current token, increment the ack count.
        if(m->i == sched->ack_token)
          sched->ack_count++;
        break;
      }

      case SCHED_TERMINATE:
      {
        sched->terminate = true;
        break;
      }

      default: {}
    }
  }
}

/**
 * If we can terminate, return true. If all schedulers are waiting, one of
 * them will stop the ASIO back end and tell the cycle detector to try to
 * terminate.
 */
static bool quiescent(scheduler_t* sched, uint64_t tsc, uint64_t tsc2)
{
  read_msg(sched);

  if(sched->terminate)
    return true;

  if(sched->ack_count == scheduler_count)
  {
    if(sched->asio_stopped)
    {
      // Reset the ACK token in case we are rescheduling ourself.
      cycle_terminate(&sched->ctx);
      sched->ack_token++;
      sched->ack_count = 0;
    } else if(asio_stop()) {
      sched->asio_stopped = true;
      sched->ack_token++;
      sched->ack_count = 0;

      // Run another CNF/ACK cycle.
      for(uint32_t i = 0; i < scheduler_count; i++)
        send_msg(i, SCHED_CNF, sched->ack_token);
    }
  }

  cpu_core_pause(tsc, tsc2, use_yield);
  return false;
}

static scheduler_t* choose_victim(scheduler_t* sched)
{
  scheduler_t* victim = sched->last_victim;

  while(true)
  {
    // Back up one.
    victim--;

    // Wrap around to the end.
    if(victim < scheduler)
      victim = &scheduler[scheduler_count - 1];

    if(victim == sched->last_victim)
    {
      // If we have tried all possible victims, return no victim. Set our last
      // victim to ourself to indicate we've started over.
      sched->last_victim = sched;
      break;
    }

    // Don't try to steal from ourself.
    if(victim == sched)
      continue;

    // Record that this is our victim and return it.
    sched->last_victim = victim;
    return victim;
  }

  return NULL;
}

/**
 * Use mpmcqs to allow stealing directly from a victim, without waiting for a
 * response.
 */
static pony_actor_t* steal(scheduler_t* sched, pony_actor_t* prev)
{
  send_msg(0, SCHED_BLOCK, 0);
  uint64_t tsc = __pony_rdtsc();
  pony_actor_t* actor;

  while(true)
  {
    scheduler_t* victim = choose_victim(sched);

    if(victim == NULL)
      victim = sched;

    actor = pop_global(victim);

    if(actor != NULL)
      break;

    uint64_t tsc2 = __pony_rdtsc();

    if(quiescent(sched, tsc, tsc2))
      return NULL;

    // If we have been passed an actor (implicitly, the cycle detector), and
    // enough time has elapsed without stealing or quiescing, return the actor
    // we were passed (allowing the cycle detector to run).
    if((prev != NULL) && ((tsc2 - tsc) > 10000000000))
    {
      actor = prev;
      break;
    }
  }

  send_msg(0, SCHED_UNBLOCK, 0);
  return actor;
}

/**
 * Run a scheduler thread until termination.
 */
static void run(scheduler_t* sched)
{
  pony_actor_t* actor = pop_global(sched);

  while(true)
  {
    if(actor == NULL)
    {
      // We had an empty queue and no rescheduled actor.
      actor = steal(sched, NULL);

      if(actor == NULL)
      {
        // Termination.
        assert(pop(sched) == NULL);
        return;
      }
    }

    // Run the current actor and get the next actor.
    bool reschedule = actor_run(&sched->ctx, actor);
    pony_actor_t* next = pop_global(sched);

    if(reschedule)
    {
      if(next != NULL)
      {
        // If we have a next actor, we go on the back of the queue. Otherwise,
        // we continue to run this actor.
        push(sched, actor);
        actor = next;
      } else if(is_cycle(actor)) {
        // If all we have is the cycle detector, try to steal something else to
        // run as well.
        next = steal(sched, actor);

        if(next == NULL)
        {
          // Termination.
          return;
        }

        // Push the cycle detector and run the actor we stole.
        if(actor != next)
        {
          push(sched, actor);
          actor = next;
        }
      }
    } else {
      // We aren't rescheduling, so run the next actor. This may be NULL if our
      // queue was empty.
      actor = next;
    }
  }
}

static DECLARE_THREAD_FN(run_thread)
{
  scheduler_t* sched = (scheduler_t*) arg;
  this_scheduler = sched;
  cpu_affinity(sched->cpu);
  run(sched);

  return 0;
}

static void scheduler_shutdown()
{
  uint32_t start;

  if(scheduler[0].tid == pony_thread_self())
    start = 1;
  else
    start = 0;

  for(uint32_t i = start; i < scheduler_count; i++)
    pony_thread_join(scheduler[i].tid);

#ifdef USE_TELEMETRY
  printf("\"telemetry\": [\n");
#endif

  for(uint32_t i = 0; i < scheduler_count; i++)
  {
    while(messageq_pop(&scheduler[i].mq) != NULL);
    messageq_destroy(&scheduler[i].mq);
    mpmcq_destroy(&scheduler[i].q);

#ifdef USE_TELEMETRY
    pony_ctx_t* ctx = &scheduler[i].ctx;

    printf(
      "  {\n"
      "    \"count_gc_passes\": " __zu ",\n"
      "    \"count_alloc\": " __zu ",\n"
      "    \"count_alloc_size\": " __zu ",\n"
      "    \"count_alloc_actors\": " __zu ",\n"
      "    \"count_msg_app\": " __zu ",\n"
      "    \"count_msg_block\": " __zu ",\n"
      "    \"count_msg_unblock\": " __zu ",\n"
      "    \"count_msg_acquire\": " __zu ",\n"
      "    \"count_msg_release\": " __zu ",\n"
      "    \"count_msg_conf\": " __zu ",\n"
      "    \"count_msg_ack\": " __zu ",\n"
      "    \"time_in_gc\": " __zu ",\n"
      "    \"time_in_send_scan\": " __zu ",\n"
      "    \"time_in_recv_scan\": " __zu "\n"
      "  }",
      ctx->count_gc_passes,
      ctx->count_alloc,
      ctx->count_alloc_size,
      ctx->count_alloc_actors,
      ctx->count_msg_app,
      ctx->count_msg_block,
      ctx->count_msg_unblock,
      ctx->count_msg_acquire,
      ctx->count_msg_release,
      ctx->count_msg_conf,
      ctx->count_msg_ack,
      ctx->time_in_gc,
      ctx->time_in_send_scan,
      ctx->time_in_recv_scan
      );

    if(i < (scheduler_count - 1))
      printf(",\n");
#endif
  }

#ifdef USE_TELEMETRY
  printf("\n]\n");
#endif

  pool_free_size(scheduler_count * sizeof(scheduler_t), scheduler);
  scheduler = NULL;
  scheduler_count = 0;

  mpmcq_destroy(&inject);
}

pony_ctx_t* scheduler_init(uint32_t threads, bool noyield)
{
  use_yield = !noyield;

  // If no thread count is specified, use the available physical core count.
  if(threads == 0)
    threads = cpu_count();

  scheduler_count = threads;
  scheduler = (scheduler_t*)pool_alloc_size(
    scheduler_count * sizeof(scheduler_t));
  memset(scheduler, 0, scheduler_count * sizeof(scheduler_t));

  cpu_assign(scheduler_count, scheduler);

  for(uint32_t i = 0; i < scheduler_count; i++)
  {
    scheduler[i].ctx.scheduler = &scheduler[i];
    scheduler[i].last_victim = &scheduler[i];
    messageq_init(&scheduler[i].mq);
    mpmcq_init(&scheduler[i].q);
  }

  this_scheduler = &scheduler[0];
  mpmcq_init(&inject);
  asio_init();

  return &scheduler[0].ctx;
}

bool scheduler_start(bool library)
{
  this_scheduler = NULL;

  if(!asio_start())
    return false;

  detect_quiescence = !library;

  uint32_t start;

  if(library)
  {
    start = 0;
  } else {
    start = 1;
    scheduler[0].tid = pony_thread_self();
  }

  for(uint32_t i = start; i < scheduler_count; i++)
  {
    if(!pony_thread_create(&scheduler[i].tid, run_thread, scheduler[i].cpu,
      &scheduler[i]))
      return false;
  }

  if(!library)
  {
    run_thread(&scheduler[0]);
    scheduler_shutdown();
  }

  return true;
}

void scheduler_stop()
{
  _atomic_store(&detect_quiescence, true);
  scheduler_shutdown();
}

void scheduler_add(pony_ctx_t* ctx, pony_actor_t* actor)
{
  if(ctx != NULL)
  {
    // Add to the current scheduler thread.
    push(ctx->scheduler, actor);
  } else {
    // Put on the shared mpmcq.
    mpmcq_push(&inject, actor);
  }
}

void scheduler_terminate()
{
  for(uint32_t i = 0; i < scheduler_count; i++)
    send_msg(i, SCHED_TERMINATE, 0);
}

uint32_t scheduler_cores()
{
  return scheduler_count;
}

pony_ctx_t* pony_ctx()
{
  return &this_scheduler->ctx;
}
