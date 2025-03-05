#define PONY_WANT_ATOMIC_DEFS

#include "tracing.h"

#include <fnmatch.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ponyassert.h"
#include "../asio/asio.h"
#include "../sched/cpu.h"
#include "../sched/scheduler.h"
#include "../mem/pool.h"
#include "../actor/actor.h"
#if defined(__GLIBC__) || defined(PLATFORM_IS_BSD) || defined(ALPINE_LINUX) || defined(PLATFORM_IS_MACOSX)
#  include <execinfo.h>
#endif

#if defined(USE_RUNTIME_TRACING)

#define PONY_TRACING_THREAD_INDEX -998
#define PONY_TRACING_MAX_STACK_FRAME_DEPTH 256

#if defined(PLATFORM_IS_BSD) || defined(PLATFORM_IS_MACOSX)
#define PONY_TRACING_PAUSE_THREAD_SIGNAL SIGINFO
#define PONY_TRACING_SLEEP_WAKE_SIGNAL SIGUSR2
#else
#define PONY_TRACING_PAUSE_THREAD_SIGNAL SIGRTMIN
#define PONY_TRACING_SLEEP_WAKE_SIGNAL SIGUSR2
#endif

#if defined(PLATFORM_IS_BSD)
typedef size_t stack_depth_t;
#else
typedef int stack_depth_t;
#endif

typedef enum
{
  TRACING_CATEGORY_SCHEDULER = 1 << 0,
  TRACING_CATEGORY_SCHEDULER_MESSAGING = 1 << 1,
  TRACING_CATEGORY_ACTOR = 1 << 2,
  TRACING_CATEGORY_ACTOR_BEHAVIOR = 1 << 3,
  TRACING_CATEGORY_ACTOR_GC = 1 << 4,
  TRACING_CATEGORY_ACTOR_STATE_CHANGE = 1 << 5,
  TRACING_CATEGORY_SYSTEMATIC_TESTING = 1 << 6,
  TRACING_CATEGORY_SYSTEMATIC_TESTING_DETAILS = 1 << 7,
} tracing_category_t;

typedef enum
{
  TRACING_MSG_THREAD_START,
  TRACING_MSG_THREAD_STOP,
  TRACING_MSG_THREAD_SUSPEND,
  TRACING_MSG_THREAD_RESUME,
  TRACING_MSG_THREAD_RECEIVE_MESSAGE,
  TRACING_MSG_THREAD_SEND_MESSAGE,
  TRACING_MSG_ACTOR_CREATED,
  TRACING_MSG_ACTOR_DESTROYED,
  TRACING_MSG_ACTOR_RUN_START,
  TRACING_MSG_ACTOR_RUN_STOP,
  TRACING_MSG_ACTOR_BEHAVIOR_RUN_START,
  TRACING_MSG_ACTOR_BEHAVIOR_RUN_STOP,
  TRACING_MSG_ACTOR_GC_MARK_START,
  TRACING_MSG_ACTOR_GC_MARK_END,
  TRACING_MSG_ACTOR_GC_SWEEP_START,
  TRACING_MSG_ACTOR_GC_SWEEP_END,
  TRACING_MSG_ACTOR_MUTED,
  TRACING_MSG_ACTOR_UNMUTED,
  TRACING_MSG_ACTOR_OVERLOADED,
  TRACING_MSG_ACTOR_NOTOVERLOADED,
  TRACING_MSG_ACTOR_UNDERPRESSURE,
  TRACING_MSG_ACTOR_NOTUNDERPRESSURE,
  TRACING_MSG_ACTOR_BLOCKED,
  TRACING_MSG_ACTOR_UNBLOCKED,
  TRACING_MSG_ACTOR_TRACING_ENABLED,
  TRACING_MSG_ACTOR_TRACING_DISABLED,
  TRACING_MSG_SYSTEMATIC_TESTING_CONFIG,
  TRACING_MSG_SYSTEMATIC_TESTING_STARTED,
  TRACING_MSG_SYSTEMATIC_TESTING_FINISHED,
  TRACING_MSG_SYSTEMATIC_TESTING_WAITING_TO_START_BEGIN,
  TRACING_MSG_SYSTEMATIC_TESTING_WAITING_TO_START_END,
  TRACING_MSG_SYSTEMATIC_TESTING_TIMESLICE_BEGIN,
  TRACING_MSG_SYSTEMATIC_TESTING_TIMESLICE_END,
  TRACING_MSG_FLIGHT_RECORDER_DUMP,
} tracing_msg_type_t;

typedef enum
{
  TRACING_EVENT_INSTANT,
  TRACING_EVENT_OBJECT_CREATED,
  TRACING_EVENT_OBJECT_SNAPSHOT,
  TRACING_EVENT_OBJECT_DESTROYED,
  TRACING_EVENT_BEGIN,
  TRACING_EVENT_END,
  TRACING_EVENT_CONTEXT_ENTER,
  TRACING_EVENT_CONTEXT_LEAVE,
  TRACING_EVENT_ASYNC_BEGIN,
  TRACING_EVENT_ASYNC_INSTANT,
  TRACING_EVENT_ASYNC_END,
  TRACING_EVENT_COUNTER,
} tracing_event_t;

typedef enum
{
  TRACING_EVENT_INSTANT_SCOPE_THREAD,
  TRACING_EVENT_INSTANT_SCOPE_PROCESS,
  TRACING_EVENT_INSTANT_SCOPE_GLOBAL,
} tracing_event_instant_scope_t;


// TODO: reduce duplication in struct definitions maybe so the same change doesn't have to be made in a 1000 places
typedef struct tracing_thread_start_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  int32_t index;
} tracing_thread_start_t;

typedef struct tracing_thread_stop_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  int32_t index;
#ifdef USE_RUNTIMESTATS
  schedulerstats_t schedulerstats;
#endif
} tracing_thread_stop_t;

typedef struct tracing_thread_suspend_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  int32_t index;
} tracing_thread_suspend_t;

typedef struct tracing_thread_resume_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  int32_t index;
#ifdef USE_RUNTIMESTATS
  schedulerstats_t schedulerstats;
#endif
} tracing_thread_resume_t;

typedef struct tracing_thread_receive_message_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  int32_t index;
  sched_msg_t msg_type;
  intptr_t arg;
} tracing_thread_receive_message_t;

typedef struct tracing_thread_send_message_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  int32_t index;
  int32_t to_sched_index;
  sched_msg_t msg_type;
  intptr_t arg;
} tracing_thread_send_message_t;

typedef struct tracing_actor_created_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  pony_actor_t* actor;
  pony_type_t* type;
  uint8_t internal_flags;
  uint8_t sync_flags;
} tracing_actor_created_t;

typedef struct tracing_actor_destroyed_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  pony_actor_t* actor;
  pony_type_t* type;
  uint8_t internal_flags;
  uint8_t sync_flags;
  size_t heap_next_gc;
  size_t heap_used;
  size_t gc_rc;
  uint32_t live_asio_events;
#ifdef USE_RUNTIMESTATS
  actorstats_t actorstats;
#endif
} tracing_actor_destroyed_t;

typedef struct tracing_actor_run_start_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  pony_actor_t* actor;
  pony_type_t* type;
  uint8_t internal_flags;
  uint8_t sync_flags;
} tracing_actor_run_start_t;

typedef struct tracing_actor_run_stop_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  pony_actor_t* actor;
  pony_type_t* type;
  uint8_t internal_flags;
  uint8_t sync_flags;
  size_t heap_next_gc;
  size_t heap_used;
  size_t gc_rc;
  uint32_t live_asio_events;
#ifdef USE_RUNTIMESTATS
  actorstats_t actorstats;
#endif
} tracing_actor_run_stop_t;

typedef struct tracing_actor_behavior_run_start_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  pony_actor_t* actor;
  pony_type_t* type;
  uint32_t behavior_id;
  uint8_t internal_flags;
  uint8_t sync_flags;
} tracing_actor_behavior_run_start_t;

typedef struct tracing_actor_behavior_run_end_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  pony_actor_t* actor;
  pony_type_t* type;
  uint32_t behavior_id;
  uint8_t internal_flags;
  uint8_t sync_flags;
} tracing_actor_behavior_run_end_t;

typedef struct tracing_actor_gc_mark_start_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  pony_actor_t* actor;
  pony_type_t* type;
} tracing_actor_gc_mark_start_t;

typedef struct tracing_actor_gc_mark_end_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  pony_actor_t* actor;
  pony_type_t* type;
} tracing_actor_gc_mark_end_t;

typedef struct tracing_actor_gc_sweep_start_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  pony_actor_t* actor;
  pony_type_t* type;
} tracing_actor_gc_sweep_start_t;

typedef struct tracing_actor_gc_sweep_end_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  pony_actor_t* actor;
  pony_type_t* type;
} tracing_actor_gc_sweep_end_t;

typedef struct tracing_actor_muted_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  pony_actor_t* actor;
  pony_type_t* type;
  uint8_t internal_flags;
  uint8_t sync_flags;
} tracing_actor_muted_t;

typedef struct tracing_actor_unmuted_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  pony_actor_t* actor;
  pony_type_t* type;
  uint8_t internal_flags;
  uint8_t sync_flags;
} tracing_actor_unmuted_t;

typedef struct tracing_actor_overloaded_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  pony_actor_t* actor;
  pony_type_t* type;
  uint8_t internal_flags;
  uint8_t sync_flags;
} tracing_actor_overloaded_t;

typedef struct tracing_actor_notoverloaded_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  pony_actor_t* actor;
  pony_type_t* type;
  uint8_t internal_flags;
  uint8_t sync_flags;
} tracing_actor_notoverloaded_t;

typedef struct tracing_actor_underpressure_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  pony_actor_t* actor;
  pony_type_t* type;
  uint8_t internal_flags;
  uint8_t sync_flags;
} tracing_actor_underpressure_t;

typedef struct tracing_actor_notunderpressure_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  pony_actor_t* actor;
  pony_type_t* type;
  uint8_t internal_flags;
  uint8_t sync_flags;
} tracing_actor_notunderpressure_t;

typedef struct tracing_actor_blocked_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  pony_actor_t* actor;
  pony_type_t* type;
  uint8_t internal_flags;
  uint8_t sync_flags;
} tracing_actor_blocked_t;

typedef struct tracing_actor_unblocked_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  pony_actor_t* actor;
  pony_type_t* type;
  uint8_t internal_flags;
  uint8_t sync_flags;
} tracing_actor_unblocked_t;

typedef struct tracing_actor_tracing_enabled_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  pony_actor_t* actor;
  pony_type_t* type;
  uint8_t internal_flags;
  uint8_t sync_flags;
  size_t heap_next_gc;
  size_t heap_used;
  size_t gc_rc;
  uint32_t live_asio_events;
#ifdef USE_RUNTIMESTATS
  actorstats_t actorstats;
#endif
} tracing_actor_tracing_enabled_t;

typedef struct tracing_actor_tracing_disabled_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  pony_actor_t* actor;
  pony_type_t* type;
  uint8_t internal_flags;
  uint8_t sync_flags;
  size_t heap_next_gc;
  size_t heap_used;
  size_t gc_rc;
  uint32_t live_asio_events;
#ifdef USE_RUNTIMESTATS
  actorstats_t actorstats;
#endif
} tracing_actor_tracing_disabled_t;

typedef struct tracing_systematic_testing_config_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  uint64_t random_seed;
} tracing_systematic_testing_config_t;

typedef struct tracing_systematic_testing_started_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
} tracing_systematic_testing_started_t;

typedef struct tracing_systematic_testing_finished_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
} tracing_systematic_testing_finished_t;

typedef struct tracing_systematic_testing_waiting_to_start_begin_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
} tracing_systematic_testing_waiting_to_start_begin_t;

typedef struct tracing_systematic_testing_waiting_to_start_end_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
} tracing_systematic_testing_waiting_to_start_end_t;

typedef struct tracing_systematic_testing_timeslice_begin_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
} tracing_systematic_testing_timeslice_begin_t;

typedef struct tracing_systematic_testing_timeslice_end_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
} tracing_systematic_testing_timeslice_end_t;

typedef struct tracing_flight_recorder_dump_t
{
  pony_msg_t msg;
  uint64_t thread_id;
  uint64_t ts;
  int32_t index;
  pony_actor_t* actor;
  pony_type_t* type;
  uint8_t internal_flags;
  uint8_t sync_flags;
  size_t heap_next_gc;
  size_t heap_used;
  size_t gc_rc;
  uint32_t live_asio_events;
  siginfo_t* siginfo;
  stack_depth_t num_stack_frames;
  void* stack_frames[PONY_TRACING_MAX_STACK_FRAME_DEPTH];
} tracing_flight_recorder_dump_t;

typedef struct tracing_scheduler_t
{
  uint64_t tid;
  scheduler_t* sched;
  uint64_t flight_recorder_event_idx;
  pony_msg_t** flight_recorder_events;
} tracing_scheduler_t;


static PONY_ATOMIC(bool) tracing_suspend_changing;
static PONY_ATOMIC(bool) handling_flight_recorder_dump;
static PONY_ATOMIC(bool) flight_recorder_dump_completed;
static PONY_ATOMIC(bool) tracing_thread_suspended;
static PONY_ATOMIC(bool) tracing_thread_suspended_check;
static uint64_t enabled_categories_bitmask;
static char* output_format;
static char* output_file;
static char* tracing_mode;
static FILE *log_file;
static scheduler_t tracing_thread;
static PONY_ATOMIC(bool) stop_tracing_thread;
static pid_t pid;
static bool force_actor_tracing_enabled;
static bool flight_recorder_enabled;
static bool flight_recorder_handle_term_int;
static size_t flight_recorder_max_events;
static uint32_t total_sched_count;
static tracing_scheduler_t* tracing_schedulers;
static tracing_flight_recorder_dump_t flight_recorder_dump_message;
static __pony_thread_local tracing_scheduler_t* this_tracing_scheduler;

// TODO: figure out if time source should be something else or configurable
static uint64_t get_time_nanos()
{
#if defined PLATFORM_IS_ARM
# if defined(__APPLE__)
  return clock_gettime_nsec_np(CLOCK_UPTIME_RAW)/1000;
# else
#   if defined(PLATFORM_IS_LINUX)
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  return (ts.tv_sec * 1000000000) + (ts.tv_nsec);
#   else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (ts.tv_sec * 1000000000) + (ts.tv_nsec);
#   endif
# endif
#elif defined PLATFORM_IS_X86
#   if defined(PLATFORM_IS_LINUX)
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  return (ts.tv_sec * 1000000000) + (ts.tv_nsec);
#   else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (ts.tv_sec * 1000000000) + (ts.tv_nsec);
#   endif
#endif
}

static uint64_t convert_time_nanos_to_micros(uint64_t nanos)
{
  return nanos/1000;
}

// used to "pause" all scheduler threads if a trap is encountered
static void pause_threads_signal_handler(int sig, siginfo_t* siginfo, void* context)
{
  (void) sig;
  (void) context;

  if(siginfo->si_pid != pid)
  {
    // if the signal didn't come from this process, ignore it
    return;
  }

  // sleep for a bunch to hang this thread until program exits
  sleep(100000);
}

static void flight_recorder_dump_signal_handler(int sig, siginfo_t* siginfo, void* context)
{
  (void) context;

  // make sure we only handle the first signal if many get generated in multiple schedulers
  if(!atomic_exchange_explicit(&handling_flight_recorder_dump, true,
    memory_order_acquire))
  {
    // try to pause all threads to ensure no new events get generated
    for(uint32_t i = 0; i < total_sched_count; i++)
    {
      tracing_scheduler_t* t_sched = &tracing_schedulers[i];
      if(t_sched != this_tracing_scheduler)
      {
        if(t_sched->sched != NULL)
          pthread_kill(t_sched->sched->tid, PONY_TRACING_PAUSE_THREAD_SIGNAL);
      }
    }

    flight_recorder_dump_message.msg.id = TRACING_MSG_FLIGHT_RECORDER_DUMP;
    flight_recorder_dump_message.msg.index = -1;
    flight_recorder_dump_message.siginfo = siginfo;
    flight_recorder_dump_message.index = this_tracing_scheduler->sched->index;
    flight_recorder_dump_message.thread_id = this_tracing_scheduler->tid;
    flight_recorder_dump_message.ts = get_time_nanos();
    flight_recorder_dump_message.actor = this_tracing_scheduler->sched->ctx.current;
    if(this_tracing_scheduler->sched->ctx.current != NULL)
    {
      flight_recorder_dump_message.type = this_tracing_scheduler->sched->ctx.current->type;
      flight_recorder_dump_message.internal_flags = this_tracing_scheduler->sched->ctx.current->internal_flags;
      flight_recorder_dump_message.sync_flags = atomic_load_explicit(&this_tracing_scheduler->sched->ctx.current->sync_flags, memory_order_relaxed);
      flight_recorder_dump_message.heap_next_gc = this_tracing_scheduler->sched->ctx.current->heap.next_gc;
      flight_recorder_dump_message.heap_used = this_tracing_scheduler->sched->ctx.current->heap.used;
      flight_recorder_dump_message.gc_rc = this_tracing_scheduler->sched->ctx.current->gc.rc;
      flight_recorder_dump_message.live_asio_events = this_tracing_scheduler->sched->ctx.current->live_asio_events;
    }
  
    // `backtrace` is not async signal safe, but according to the man page that's
    // only if libgcc isn't already loaded. we ensure it is by calling `backtrace`
    // manually before we set the signal handler to make sure it is safe to call here.
    flight_recorder_dump_message.num_stack_frames = backtrace(flight_recorder_dump_message.stack_frames, PONY_TRACING_MAX_STACK_FRAME_DEPTH);

    ponyint_thread_messageq_push(&tracing_thread.mq, (pony_msg_t*)(&flight_recorder_dump_message), (pony_msg_t*)(&flight_recorder_dump_message)
      #ifdef USE_DYNAMIC_TRACE
          , this_tracing_scheduler->sched->index, this_tracing_scheduler->sched->index
      #endif
          );

    // wake the tracing thread to handle the flight recorder dump
    pthread_kill(tracing_thread.tid, PONY_TRACING_SLEEP_WAKE_SIGNAL);

    uint8_t sleep_counter = 0;

    while(!atomic_load_explicit(&flight_recorder_dump_completed, memory_order_relaxed)
      // only wait 15 seconds for tracing thread flight recorder dump to finish before giving up
      && (sleep_counter < 15))
    {
      // wait for the tracing thread to finish handling the flight recorder dump
      // before we continue and let the default signal handler do its magic
      sleep(1);
      sleep_counter++;

      // wake the tracing thread to handle the flight recorder dump if necessary
      if(atomic_load_explicit(&tracing_thread_suspended, memory_order_relaxed))
        pthread_kill(tracing_thread.tid, PONY_TRACING_SLEEP_WAKE_SIGNAL);
    }

    if(sleep_counter >= 15)
    {
      // if we timed out waiting for the flight recorder dump to finish, we
      // should exit the program immediately. This is a safety measure in case
      // something went wrong in the tracing thread dump process.
      char* strsig = strsignal(sig);
      char* msg = "Flight recorder dump timed out.\n Exiting program without waiting for it to complete.\n Signal encountered: ";
      write(1, msg, strlen(msg));
      write(1, strsig, strlen(strsig));
      write(1, "\n", 1);
    }

    // reset the signal handler to the default for this signal type so that the
    // system can generate a normal core dump when we raise the signal again
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;

    sigaction(sig, &sa, NULL);
    raise(sig);
  } else {
    // if we are already handling a flight recorder dump, ignore the signal and
    // sleep for a bunch to hang this thread until program exits
    sleep(100000);
  }
}

static void wake_suspended_tracing_thread()
{
  while(atomic_load_explicit(&tracing_thread_suspended_check, memory_order_relaxed))
  {
    // get the bool that controls modifying the active scheduler count variable
    // if using signals
    if(!atomic_load_explicit(&tracing_suspend_changing, memory_order_relaxed)
      && !atomic_exchange_explicit(&tracing_suspend_changing, true,
      memory_order_acquire))
    {
      atomic_store_explicit(&tracing_thread_suspended, false, memory_order_relaxed);

      // unlock the bool that controls modifying the tracing suspend boolean
      // variable if using signals.
      atomic_store_explicit(&tracing_suspend_changing, false,
        memory_order_release);
    }

    pthread_kill(tracing_thread.tid, PONY_TRACING_SLEEP_WAKE_SIGNAL);

    // wait for the sleeping thread to wake and update check variable
    while(atomic_load_explicit(&tracing_thread_suspended_check, memory_order_relaxed))
    {
      // send signals to the tracing thread that should be awake
      // this is somewhat wasteful if the tracing thread is already awake
      // but is necessary in case the signal to wake the thread was missed
      // NOTE: this intentionally allows for the case where the tracing
      // thread might miss the signal and not wake up. That is handled
      // by a combination of the check variable and this while loop
      pthread_kill(tracing_thread.tid, PONY_TRACING_SLEEP_WAKE_SIGNAL);
    }
  }
}

static void perhaps_suspend_tracing_thread()
{
  // if we're not terminating
  // and dynamic scheduler scaling is not disabled for shutdown
  if (
    // try and get the bool that controls modifying the pinned_actor_scheduler_suspended
    // variable if using signals
    (!atomic_load_explicit(&tracing_suspend_changing, memory_order_relaxed)
      && !atomic_exchange_explicit(&tracing_suspend_changing, true,
      memory_order_acquire))
    )
  {
    atomic_store_explicit(&tracing_thread_suspended, true, memory_order_relaxed);
    atomic_store_explicit(&tracing_thread_suspended_check, true, memory_order_relaxed);

    // unlock the bool that controls modifying the tracing_thread_suspended
    // variable if using signals
    atomic_store_explicit(&tracing_suspend_changing, false,
      memory_order_release);

    // suspend and wait for signal to wake up again
    int sig;
    sigset_t sigmask;
    sigemptyset(&sigmask);         /* zero out all bits */
    sigaddset(&sigmask, PONY_TRACING_SLEEP_WAKE_SIGNAL);   /* unblock desired signal */

    // sleep waiting for signal to wake up again
    sigwait(&sigmask, &sig);

    // When using signals, need to acquire sched count changing variable
    while (true)
    {
      // get the bool that controls modifying the pinned_actor_scheduler_suspended
      // variable if using signals
      if(!atomic_load_explicit(&tracing_suspend_changing, memory_order_relaxed)
        && !atomic_exchange_explicit(&tracing_suspend_changing, true,
        memory_order_acquire))
      {

        atomic_store_explicit(&tracing_thread_suspended, false, memory_order_relaxed);
        atomic_store_explicit(&tracing_thread_suspended_check, false, memory_order_relaxed);

        // unlock the bool that controls modifying the tracing_thread_suspended
        // variable if using signals
        atomic_store_explicit(&tracing_suspend_changing, false,
          memory_order_release);

        // break while loop
        break;
      }
    }
  } else {
    // unable to get the lock to suspend so sleep for a bit
    sleep(0);
  }
}

static char* get_tracing_category_string(tracing_category_t category)
{
  switch(category)
  {
    case TRACING_CATEGORY_SCHEDULER:
      return "scheduler";
    case TRACING_CATEGORY_SCHEDULER_MESSAGING:
      return "scheduler_messaging";
    case TRACING_CATEGORY_ACTOR:
      return "actor";
    case TRACING_CATEGORY_ACTOR_BEHAVIOR:
      return "actor_behavior";
    case TRACING_CATEGORY_ACTOR_GC:
      return "actor_gc";
    case TRACING_CATEGORY_ACTOR_STATE_CHANGE:
      return "actor_state_change";
    case TRACING_CATEGORY_SYSTEMATIC_TESTING:
      return "systematic_testing";
    case TRACING_CATEGORY_SYSTEMATIC_TESTING_DETAILS:
      return "systematic_testing_details";
    default:
      pony_assert(0);
      return NULL;
  }
}

static char* get_tracing_event_string(tracing_event_t event)
{
  switch(event)
  {
    case TRACING_EVENT_INSTANT:
      return "i";
    case TRACING_EVENT_OBJECT_CREATED:
      return "N";
    case TRACING_EVENT_OBJECT_SNAPSHOT:
      return "O";
    case TRACING_EVENT_OBJECT_DESTROYED:
      return "D";
    case TRACING_EVENT_BEGIN:
      return "B";
    case TRACING_EVENT_END:
      return "E";
    case TRACING_EVENT_CONTEXT_ENTER:
      return "(";
    case TRACING_EVENT_CONTEXT_LEAVE:
      return ")";
    case TRACING_EVENT_ASYNC_BEGIN:
      return "b";
    case TRACING_EVENT_ASYNC_INSTANT:
      return "n";
    case TRACING_EVENT_ASYNC_END:
      return "e";
    case TRACING_EVENT_COUNTER:
      return "C";
    default:
      pony_assert(0);
      return NULL;
  }
}

static char* get_tracing_event_instant_scope_string(tracing_event_instant_scope_t scope)
{
  switch(scope)
  {
    case TRACING_EVENT_INSTANT_SCOPE_GLOBAL:
      return "g";
    case TRACING_EVENT_INSTANT_SCOPE_PROCESS:
      return "p";
    case TRACING_EVENT_INSTANT_SCOPE_THREAD:
      return "t";
    default:
      pony_assert(0);
      return NULL;
  }
}

static char* get_thread_name(int32_t index)
{
  switch(index)
  {
    case PONY_ASIO_SCHEDULER_INDEX:
      return "ASIO SCHEDULER THREAD";
    case PONY_UNKNOWN_SCHEDULER_INDEX:
      return "UNKNOWN SCHEDULER THREAD";
    case PONY_PINNED_ACTOR_THREAD_INDEX:
      return "PINNED ACTOR SCHEDULER THREAD";
    case PONY_TRACING_THREAD_INDEX:
      return "TRACING THREAD";
    default:
      return "SCHEDULER THREAD";
  }
}

static char* get_scheduler_message_type_string(sched_msg_t msg_type)
{
  switch(msg_type)
  {
    case SCHED_BLOCK:
      return "BLOCK";
    case SCHED_UNBLOCK:
      return "UNBLOCK";
    case SCHED_CNF:
      return "CNF";
    case SCHED_ACK:
      return "ACK";
    case SCHED_TERMINATE:
      return "TERMINATE";
    case SCHED_UNMUTE_ACTOR:
      return "UNMUTE_ACTOR";
    case SCHED_NOISY_ASIO:
      return "NOISY_ASIO";
    case SCHED_UNNOISY_ASIO:
      return "UNNOISY_ASIO";
    default:
      pony_assert(0);
      return NULL;
  }
}

static char* get_actor_behavior_string(pony_behavior_name_fn get_behavior_name_fn, uint32_t behavior_id)
{
  switch(behavior_id)
  {
    case ACTORMSG_ACK:
      return "SYS: ACK";
    case ACTORMSG_ACQUIRE:
      return "SYS: ACQUIRE";
    case ACTORMSG_BLOCK:
      return "SYS: BLOCK";
    case ACTORMSG_CHECKBLOCKED:
      return "SYS: CHECKBLOCKED";
    case ACTORMSG_CONF:
      return "SYS: CONF";
    case ACTORMSG_ISBLOCKED:
      return "SYS: ISBLOCKED";
    case ACTORMSG_RELEASE:
      return "SYS: RELEASE";
    case ACTORMSG_UNBLOCK:
      return "SYS: UNBLOCK";
    default:
    // get the behavior name from the type
      return get_behavior_name_fn(behavior_id);
  }
}

static bool tracing_category_enabled(tracing_category_t cat)
{
  return (enabled_categories_bitmask & (uint64_t)cat) != 0;
}

static void send_trace_message(pony_msg_t* msg)
{
  if(flight_recorder_enabled)
  {
    if(NULL == this_tracing_scheduler)
    {
      pony_assert(NULL != this_tracing_scheduler);

      // tracing message from an unexpected thread
      // free the message
      ponyint_pool_free(msg->index, msg);
      return;
    }

    uint64_t idx = this_tracing_scheduler->flight_recorder_event_idx & (flight_recorder_max_events - 1);

    pony_msg_t* old = this_tracing_scheduler->flight_recorder_events[idx];

    if(old != NULL)
    {
      ponyint_pool_free(old->index, old);
    }

    this_tracing_scheduler->flight_recorder_events[idx] = msg;
    this_tracing_scheduler->flight_recorder_event_idx++;
  }
  else
  {
    // TODO: batching of messages for efficiency/minimizing contention on tracing thread mailbox?
    bool was_empty = ponyint_thread_messageq_push(&tracing_thread.mq, msg, msg
  #ifdef USE_DYNAMIC_TRACE
      , this_tracing_scheduler->sched->index, this_tracing_scheduler->sched->index
  #endif
      );

    if(was_empty)
    {
      // wake tracing thread if it was sleeping because its queue was empty
      wake_suspended_tracing_thread();
    }
  }
}

void ponyint_tracing_thread_start(scheduler_t* sched)
{
  pony_assert(NULL == this_tracing_scheduler);

  uint32_t tracing_scheduler_offset = sched->index + 2;

  if(sched->index == PONY_ASIO_SCHEDULER_INDEX)
    tracing_scheduler_offset = 0;
  if(sched->index == PONY_PINNED_ACTOR_THREAD_INDEX)
    tracing_scheduler_offset = 1;

  this_tracing_scheduler = &tracing_schedulers[tracing_scheduler_offset];

  this_tracing_scheduler->sched = sched;

#if defined(PLATFORM_IS_LINUX)
  this_tracing_scheduler->tid = gettid();
#else
  pthread_threadid_np(NULL, &this_tracing_scheduler->tid);
#endif

  if(flight_recorder_enabled)
  {
    this_tracing_scheduler->flight_recorder_event_idx = 0;
    this_tracing_scheduler->flight_recorder_events = (pony_msg_t**)ponyint_pool_alloc_size(sizeof(pony_msg_t*) * flight_recorder_max_events);
    memset(this_tracing_scheduler->flight_recorder_events, 0, sizeof(pony_msg_t*) * flight_recorder_max_events);

    // linux specific: but probably not harmful on other platforms
    // call `backtrace` to load `libgcc` so that we can use it in the signal handler safely
    // per the `backtrace` man page:
    //   "If you need certain calls to these two functions to not allocate memory
    //   (in signal handlers, for  example), you need to make sure libgcc is loaded
    //   beforehand."
    void* buffer[1];
    stack_depth_t nptrs = backtrace(buffer, 1);
    char** strings = backtrace_symbols(buffer, nptrs);
    free(strings);

    // Make sure we handle signals related to pausing all threads
    struct sigaction pause_action;
    pause_action.sa_sigaction = pause_threads_signal_handler;
    // make sure to block all signals while in the signal handler
    sigfillset (&pause_action.sa_mask);
    pause_action.sa_flags = SA_SIGINFO;

    sigaction(PONY_TRACING_PAUSE_THREAD_SIGNAL, &pause_action, NULL);

    // Make sure we handle signals that we want to dump flight recorder events for
    struct sigaction fr_action;
    fr_action.sa_sigaction = flight_recorder_dump_signal_handler;
    // make sure to block all signals while in the signal handler
    sigfillset (&fr_action.sa_mask);
    fr_action.sa_flags = SA_SIGINFO;

    // register signal handlers for the fatal signals we want to handle
    sigaction(SIGABRT, &fr_action, NULL);
    sigaction(SIGBUS, &fr_action, NULL);
    sigaction(SIGFPE, &fr_action, NULL);
    sigaction(SIGILL, &fr_action, NULL);
    sigaction(SIGSEGV, &fr_action, NULL);
    sigaction(SIGSYS, &fr_action, NULL);

    // only trap SIGINT/SIGTERM if requested (might be useful for things like CI and manual testing)
    if(flight_recorder_handle_term_int)
    {
      sigaction(SIGINT, &fr_action, NULL);
      sigaction(SIGTERM, &fr_action, NULL);
    }
  }

  if(!tracing_category_enabled(TRACING_CATEGORY_SCHEDULER))
    return;

  tracing_thread_start_t* m = (tracing_thread_start_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_thread_start_t)), TRACING_MSG_THREAD_START);

  m->index = this_tracing_scheduler->sched->index;
  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_thread_stop()
{
  if(!tracing_category_enabled(TRACING_CATEGORY_SCHEDULER))
    return;

  tracing_thread_stop_t* m = (tracing_thread_stop_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_thread_stop_t)), TRACING_MSG_THREAD_STOP);

  m->index = this_tracing_scheduler->sched->index;
  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
#ifdef USE_RUNTIMESTATS
  memcpy(&m->schedulerstats, &this_tracing_scheduler->sched->ctx.schedulerstats, sizeof(schedulerstats_t));
#endif

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_thread_suspend()
{
  if(!tracing_category_enabled(TRACING_CATEGORY_SCHEDULER))
    return;

  tracing_thread_suspend_t* m = (tracing_thread_suspend_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_thread_suspend_t)), TRACING_MSG_THREAD_SUSPEND);

  m->index = this_tracing_scheduler->sched->index;
  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_thread_resume()
{
  if(!tracing_category_enabled(TRACING_CATEGORY_SCHEDULER))
    return;

  tracing_thread_resume_t* m = (tracing_thread_resume_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_thread_resume_t)), TRACING_MSG_THREAD_RESUME);

  m->index = this_tracing_scheduler->sched->index;
  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
#ifdef USE_RUNTIMESTATS
  memcpy(&m->schedulerstats, &this_tracing_scheduler->sched->ctx.schedulerstats, sizeof(schedulerstats_t));
#endif

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_thread_receive_message(sched_msg_t msg_type, intptr_t arg)
{
  if(!tracing_category_enabled(TRACING_CATEGORY_SCHEDULER_MESSAGING))
    return;

  tracing_thread_receive_message_t* m = (tracing_thread_receive_message_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_thread_receive_message_t)), TRACING_MSG_THREAD_RECEIVE_MESSAGE);

  m->index = this_tracing_scheduler->sched->index;
  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->msg_type = msg_type;
  m->arg = arg;

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_thread_send_message(sched_msg_t msg_type, intptr_t arg, int32_t from_sched_index, int32_t to_sched_index)
{
  if(!tracing_category_enabled(TRACING_CATEGORY_SCHEDULER_MESSAGING))
    return;

  (void)from_sched_index;
  pony_assert(from_sched_index == this_tracing_scheduler->sched->index);

  tracing_thread_send_message_t* m = (tracing_thread_send_message_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_thread_send_message_t)), TRACING_MSG_THREAD_SEND_MESSAGE);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->msg_type = msg_type;
  m->arg = arg;
  m->index = this_tracing_scheduler->sched->index;
  m->to_sched_index = to_sched_index;

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_actor_created(pony_actor_t* actor)
{
  if(!tracing_category_enabled(TRACING_CATEGORY_ACTOR))
    return;

  if(!force_actor_tracing_enabled && !ponyint_actor_tracing_enabled(actor))
    return;

  tracing_actor_created_t* m = (tracing_actor_created_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_actor_created_t)), TRACING_MSG_ACTOR_CREATED);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->actor = actor;
  m->type = actor->type;
  m->internal_flags = actor->internal_flags;
  m->sync_flags = atomic_load_explicit(&actor->sync_flags, memory_order_relaxed);

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_actor_destroyed(pony_actor_t* actor)
{
  if(!tracing_category_enabled(TRACING_CATEGORY_ACTOR))
    return;

  if(!force_actor_tracing_enabled && !ponyint_actor_tracing_enabled(actor))
    return;

  tracing_actor_destroyed_t* m = (tracing_actor_destroyed_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_actor_destroyed_t)), TRACING_MSG_ACTOR_DESTROYED);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->actor = actor;
  m->type = actor->type;
  m->internal_flags = actor->internal_flags;
  m->sync_flags = atomic_load_explicit(&actor->sync_flags, memory_order_relaxed);
  m->heap_next_gc = actor->heap.next_gc;
  m->heap_used = actor->heap.used;
  m->gc_rc = actor->gc.rc;
  m->live_asio_events = actor->live_asio_events;
#ifdef USE_RUNTIMESTATS
  memcpy(&m->actorstats, &actor->actorstats, sizeof(actorstats_t));
#endif

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_thread_actor_run_start(pony_actor_t* actor)
{
  pony_assert(NULL == this_tracing_scheduler->sched->ctx.current);

  if(!tracing_category_enabled(TRACING_CATEGORY_ACTOR))
    return;

  if(!force_actor_tracing_enabled && !ponyint_actor_tracing_enabled(actor))
    return;

  tracing_actor_run_start_t* m = (tracing_actor_run_start_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_actor_run_start_t)), TRACING_MSG_ACTOR_RUN_START);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->actor = actor;
  m->type = actor->type;
  m->internal_flags = actor->internal_flags;
  m->sync_flags = atomic_load_explicit(&actor->sync_flags, memory_order_relaxed);

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_thread_actor_run_stop(pony_actor_t* actor)
{
  pony_assert(actor == this_tracing_scheduler->sched->ctx.current);

  if(!tracing_category_enabled(TRACING_CATEGORY_ACTOR))
    return;

  if(!force_actor_tracing_enabled && !ponyint_actor_tracing_enabled(actor))
    return;

  tracing_actor_run_stop_t* m = (tracing_actor_run_stop_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_actor_run_stop_t)), TRACING_MSG_ACTOR_RUN_STOP);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->actor = actor;
  m->type = actor->type;
  m->internal_flags = actor->internal_flags;
  m->sync_flags = atomic_load_explicit(&actor->sync_flags, memory_order_relaxed);
  m->heap_next_gc = actor->heap.next_gc;
  m->heap_used = actor->heap.used;
  m->gc_rc = actor->gc.rc;
  m->live_asio_events = actor->live_asio_events;
#ifdef USE_RUNTIMESTATS
  memcpy(&m->actorstats, &actor->actorstats, sizeof(actorstats_t));
#endif

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_actor_behavior_run_start(pony_actor_t* actor, uint32_t behavior_id)
{
  if(!tracing_category_enabled(TRACING_CATEGORY_ACTOR_BEHAVIOR))
    return;

  if(!force_actor_tracing_enabled && !ponyint_actor_tracing_enabled(actor))
    return;

  tracing_actor_behavior_run_start_t* m = (tracing_actor_behavior_run_start_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_actor_behavior_run_start_t)), TRACING_MSG_ACTOR_BEHAVIOR_RUN_START);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->actor = actor;
  m->type = actor->type;
  m->behavior_id = behavior_id;
  m->internal_flags = actor->internal_flags;
  m->sync_flags = atomic_load_explicit(&actor->sync_flags, memory_order_relaxed);

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_actor_behavior_run_end(pony_actor_t* actor, uint32_t behavior_id)
{
  if(!tracing_category_enabled(TRACING_CATEGORY_ACTOR_BEHAVIOR))
    return;

  if(!force_actor_tracing_enabled && !ponyint_actor_tracing_enabled(actor))
    return;

  tracing_actor_behavior_run_end_t* m = (tracing_actor_behavior_run_end_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_actor_behavior_run_end_t)), TRACING_MSG_ACTOR_BEHAVIOR_RUN_STOP);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->actor = actor;
  m->type = actor->type;
  m->behavior_id = behavior_id;
  m->internal_flags = actor->internal_flags;
  m->sync_flags = atomic_load_explicit(&actor->sync_flags, memory_order_relaxed);

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_actor_gc_mark_start(pony_actor_t* actor)
{
  if(!tracing_category_enabled(TRACING_CATEGORY_ACTOR_GC))
    return;

  if(!force_actor_tracing_enabled && !ponyint_actor_tracing_enabled(actor))
    return;

  tracing_actor_gc_mark_start_t* m = (tracing_actor_gc_mark_start_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_actor_gc_mark_start_t)), TRACING_MSG_ACTOR_GC_MARK_START);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->actor = actor;
  m->type = actor->type;

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_actor_gc_mark_end(pony_actor_t* actor)
{
  if(!tracing_category_enabled(TRACING_CATEGORY_ACTOR_GC))
    return;

  if(!force_actor_tracing_enabled && !ponyint_actor_tracing_enabled(actor))
    return;

  tracing_actor_gc_mark_end_t* m = (tracing_actor_gc_mark_end_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_actor_gc_mark_end_t)), TRACING_MSG_ACTOR_GC_MARK_END);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->actor = actor;
  m->type = actor->type;

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_actor_gc_sweep_start(pony_actor_t* actor)
{
  if(!tracing_category_enabled(TRACING_CATEGORY_ACTOR_GC))
    return;

  if(!force_actor_tracing_enabled && !ponyint_actor_tracing_enabled(actor))
    return;

  tracing_actor_gc_sweep_start_t* m = (tracing_actor_gc_sweep_start_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_actor_gc_sweep_start_t)), TRACING_MSG_ACTOR_GC_SWEEP_START);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->actor = actor;
  m->type = actor->type;

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_actor_gc_sweep_end(pony_actor_t* actor)
{
  if(!tracing_category_enabled(TRACING_CATEGORY_ACTOR_GC))
    return;

  if(!force_actor_tracing_enabled && !ponyint_actor_tracing_enabled(actor))
    return;

  tracing_actor_gc_sweep_end_t* m = (tracing_actor_gc_sweep_end_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_actor_gc_sweep_end_t)), TRACING_MSG_ACTOR_GC_SWEEP_END);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->actor = actor;
  m->type = actor->type;

  send_trace_message((pony_msg_t*)m);

  if(this_tracing_scheduler->sched->index == 0)
  {
    // TODO: REMOVE ME
    int z = *((int*)1);
    printf("%d\n", z);
  }
}

void ponyint_tracing_actor_muted(pony_actor_t* actor)
{
  if(!tracing_category_enabled(TRACING_CATEGORY_ACTOR_STATE_CHANGE))
    return;

  if(!force_actor_tracing_enabled && !ponyint_actor_tracing_enabled(actor))
    return;

  tracing_actor_muted_t* m = (tracing_actor_muted_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_actor_muted_t)), TRACING_MSG_ACTOR_MUTED);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->actor = actor;
  m->type = actor->type;
  m->internal_flags = actor->internal_flags;
  m->sync_flags = atomic_load_explicit(&actor->sync_flags, memory_order_relaxed);

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_actor_unmuted(pony_actor_t* actor)
{
  if(!tracing_category_enabled(TRACING_CATEGORY_ACTOR_STATE_CHANGE))
    return;

  if(!force_actor_tracing_enabled && !ponyint_actor_tracing_enabled(actor))
    return;

  tracing_actor_unmuted_t* m = (tracing_actor_unmuted_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_actor_unmuted_t)), TRACING_MSG_ACTOR_UNMUTED);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->actor = actor;
  m->type = actor->type;
  m->internal_flags = actor->internal_flags;
  m->sync_flags = atomic_load_explicit(&actor->sync_flags, memory_order_relaxed);

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_actor_overloaded(pony_actor_t* actor)
{
  if(!tracing_category_enabled(TRACING_CATEGORY_ACTOR_STATE_CHANGE))
    return;

  if(!force_actor_tracing_enabled && !ponyint_actor_tracing_enabled(actor))
    return;

  tracing_actor_overloaded_t* m = (tracing_actor_overloaded_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_actor_overloaded_t)), TRACING_MSG_ACTOR_OVERLOADED);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->actor = actor;
  m->type = actor->type;
  m->internal_flags = actor->internal_flags;
  m->sync_flags = atomic_load_explicit(&actor->sync_flags, memory_order_relaxed);

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_actor_notoverloaded(pony_actor_t* actor)
{
  if(!tracing_category_enabled(TRACING_CATEGORY_ACTOR_STATE_CHANGE))
    return;

  if(!force_actor_tracing_enabled && !ponyint_actor_tracing_enabled(actor))
    return;

  tracing_actor_notoverloaded_t* m = (tracing_actor_notoverloaded_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_actor_notoverloaded_t)), TRACING_MSG_ACTOR_NOTOVERLOADED);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->actor = actor;
  m->type = actor->type;
  m->internal_flags = actor->internal_flags;
  m->sync_flags = atomic_load_explicit(&actor->sync_flags, memory_order_relaxed);

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_actor_underpressure(pony_actor_t* actor)
{
  if(!tracing_category_enabled(TRACING_CATEGORY_ACTOR_STATE_CHANGE))
    return;

  if(!force_actor_tracing_enabled && !ponyint_actor_tracing_enabled(actor))
    return;

  tracing_actor_underpressure_t* m = (tracing_actor_underpressure_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_actor_underpressure_t)), TRACING_MSG_ACTOR_UNDERPRESSURE);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->actor = actor;
  m->type = actor->type;
  m->internal_flags = actor->internal_flags;
  m->sync_flags = atomic_load_explicit(&actor->sync_flags, memory_order_relaxed);

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_actor_notunderpressure(pony_actor_t* actor)
{
  if(!tracing_category_enabled(TRACING_CATEGORY_ACTOR_STATE_CHANGE))
    return;

  if(!force_actor_tracing_enabled && !ponyint_actor_tracing_enabled(actor))
    return;

  tracing_actor_notunderpressure_t* m = (tracing_actor_notunderpressure_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_actor_notunderpressure_t)), TRACING_MSG_ACTOR_NOTUNDERPRESSURE);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->actor = actor;
  m->type = actor->type;
  m->internal_flags = actor->internal_flags;
  m->sync_flags = atomic_load_explicit(&actor->sync_flags, memory_order_relaxed);

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_actor_blocked(pony_actor_t* actor)
{
  if(!tracing_category_enabled(TRACING_CATEGORY_ACTOR_STATE_CHANGE))
    return;

  if(!force_actor_tracing_enabled && !ponyint_actor_tracing_enabled(actor))
    return;

  tracing_actor_blocked_t* m = (tracing_actor_blocked_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_actor_blocked_t)), TRACING_MSG_ACTOR_BLOCKED);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->actor = actor;
  m->type = actor->type;
  m->internal_flags = actor->internal_flags;
  m->sync_flags = atomic_load_explicit(&actor->sync_flags, memory_order_relaxed);

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_actor_unblocked(pony_actor_t* actor)
{
  if(!tracing_category_enabled(TRACING_CATEGORY_ACTOR_STATE_CHANGE))
    return;

  if(!force_actor_tracing_enabled && !ponyint_actor_tracing_enabled(actor))
    return;

  tracing_actor_unblocked_t* m = (tracing_actor_unblocked_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_actor_unblocked_t)), TRACING_MSG_ACTOR_UNBLOCKED);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->actor = actor;
  m->type = actor->type;
  m->internal_flags = actor->internal_flags;
  m->sync_flags = atomic_load_explicit(&actor->sync_flags, memory_order_relaxed);

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_actor_tracing_enabled(pony_actor_t* actor)
{
  if(!tracing_category_enabled(TRACING_CATEGORY_ACTOR))
    return;

  if(!force_actor_tracing_enabled && !ponyint_actor_tracing_enabled(actor))
    return;

  tracing_actor_tracing_enabled_t* m = (tracing_actor_tracing_enabled_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_actor_tracing_enabled_t)), TRACING_MSG_ACTOR_TRACING_ENABLED);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->actor = actor;
  m->type = actor->type;
  m->internal_flags = actor->internal_flags;
  m->sync_flags = atomic_load_explicit(&actor->sync_flags, memory_order_relaxed);
  m->heap_next_gc = actor->heap.next_gc;
  m->heap_used = actor->heap.used;
  m->gc_rc = actor->gc.rc;
  m->live_asio_events = actor->live_asio_events;
#ifdef USE_RUNTIMESTATS
  memcpy(&m->actorstats, &actor->actorstats, sizeof(actorstats_t));
#endif

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_actor_tracing_disabled(pony_actor_t* actor)
{
  if(!tracing_category_enabled(TRACING_CATEGORY_ACTOR))
    return;

  if(!force_actor_tracing_enabled && !ponyint_actor_tracing_enabled(actor))
    return;

  tracing_actor_tracing_disabled_t* m = (tracing_actor_tracing_disabled_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_actor_tracing_disabled_t)), TRACING_MSG_ACTOR_TRACING_DISABLED);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->actor = actor;
  m->type = actor->type;
  m->internal_flags = actor->internal_flags;
  m->sync_flags = atomic_load_explicit(&actor->sync_flags, memory_order_relaxed);
  m->heap_next_gc = actor->heap.next_gc;
  m->heap_used = actor->heap.used;
  m->gc_rc = actor->gc.rc;
  m->live_asio_events = actor->live_asio_events;
#ifdef USE_RUNTIMESTATS
  memcpy(&m->actorstats, &actor->actorstats, sizeof(actorstats_t));
#endif

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_systematic_testing_config(uint64_t random_seed)
{
  if(!tracing_category_enabled(TRACING_CATEGORY_SYSTEMATIC_TESTING))
    return;

  tracing_systematic_testing_config_t* m = (tracing_systematic_testing_config_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_systematic_testing_config_t)), TRACING_MSG_SYSTEMATIC_TESTING_CONFIG);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();
  m->random_seed = random_seed;

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_systematic_testing_started()
{
  if(!tracing_category_enabled(TRACING_CATEGORY_SYSTEMATIC_TESTING))
    return;

  tracing_systematic_testing_started_t* m = (tracing_systematic_testing_started_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_systematic_testing_started_t)), TRACING_MSG_SYSTEMATIC_TESTING_STARTED);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_systematic_testing_finished()
{
  if(!tracing_category_enabled(TRACING_CATEGORY_SYSTEMATIC_TESTING))
    return;

  tracing_systematic_testing_finished_t* m = (tracing_systematic_testing_finished_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_systematic_testing_finished_t)), TRACING_MSG_SYSTEMATIC_TESTING_FINISHED);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_systematic_testing_waiting_to_start_begin()
{
  if(!tracing_category_enabled(TRACING_CATEGORY_SYSTEMATIC_TESTING_DETAILS))
    return;

  tracing_systematic_testing_waiting_to_start_begin_t* m = (tracing_systematic_testing_waiting_to_start_begin_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_systematic_testing_waiting_to_start_begin_t)), TRACING_MSG_SYSTEMATIC_TESTING_WAITING_TO_START_BEGIN);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_systematic_testing_waiting_to_start_end()
{
  if(!tracing_category_enabled(TRACING_CATEGORY_SYSTEMATIC_TESTING_DETAILS))
    return;

  tracing_systematic_testing_waiting_to_start_end_t* m = (tracing_systematic_testing_waiting_to_start_end_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_systematic_testing_waiting_to_start_end_t)), TRACING_MSG_SYSTEMATIC_TESTING_WAITING_TO_START_END);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_systematic_testing_timeslice_begin()
{
  if(!tracing_category_enabled(TRACING_CATEGORY_SYSTEMATIC_TESTING_DETAILS))
    return;

  tracing_systematic_testing_timeslice_begin_t* m = (tracing_systematic_testing_timeslice_begin_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_systematic_testing_timeslice_begin_t)), TRACING_MSG_SYSTEMATIC_TESTING_TIMESLICE_BEGIN);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();

  send_trace_message((pony_msg_t*)m);
}

void ponyint_tracing_systematic_testing_timeslice_end()
{
  if(!tracing_category_enabled(TRACING_CATEGORY_SYSTEMATIC_TESTING_DETAILS))
    return;

  tracing_systematic_testing_timeslice_end_t* m = (tracing_systematic_testing_timeslice_end_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(tracing_systematic_testing_timeslice_end_t)), TRACING_MSG_SYSTEMATIC_TESTING_TIMESLICE_END);

  m->thread_id = this_tracing_scheduler->tid;
  m->ts = get_time_nanos();

  send_trace_message((pony_msg_t*)m);
}



static void print_header(uint64_t thread_id)
{
  fprintf(log_file, "[\n");
  fprintf(log_file, "{\"name\":\"%s\",\"ts\":%" PRIu64 ",\"cat\":\"tracing\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"s\":\"%s\",\"args\":{ \"categories_enabled\":[\"tracing\"%s%s%s%s%s%s%s%s]}}",
    "start_trace", convert_time_nanos_to_micros(get_time_nanos()), get_tracing_event_string(TRACING_EVENT_INSTANT), pid, thread_id,
    get_tracing_event_instant_scope_string(TRACING_EVENT_INSTANT_SCOPE_PROCESS),
    (tracing_category_enabled(TRACING_CATEGORY_ACTOR) ? ",\"actor\"" : ""),
    (tracing_category_enabled(TRACING_CATEGORY_ACTOR_BEHAVIOR) ? ",\"actor_behavior\"" : ""),
    (tracing_category_enabled(TRACING_CATEGORY_ACTOR_GC) ? ",\"actor_gc\"" : ""),
    (tracing_category_enabled(TRACING_CATEGORY_ACTOR_STATE_CHANGE) ? ",\"actor_state_change\"" : ""),
    (tracing_category_enabled(TRACING_CATEGORY_SCHEDULER) ? ",\"scheduler\"" : ""),
    (tracing_category_enabled(TRACING_CATEGORY_SCHEDULER_MESSAGING) ? ",\"scheduler_messaging\"" : ""),
    (tracing_category_enabled(TRACING_CATEGORY_SYSTEMATIC_TESTING) ? ",\"systematic_testing\"" : ""),
    (tracing_category_enabled(TRACING_CATEGORY_SYSTEMATIC_TESTING_DETAILS) ? ",\"systematic_testing_details\"" : ""));
}

static void print_footer(uint64_t thread_id)
{
  fprintf(log_file, ",\n{\"name\":\"%s\",\"ts\":%" PRIu64 ",\"cat\":\"tracing\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"s\":\"%s\"}", "end_trace", convert_time_nanos_to_micros(get_time_nanos()), get_tracing_event_string(TRACING_EVENT_INSTANT), pid, thread_id, get_tracing_event_instant_scope_string(TRACING_EVENT_INSTANT_SCOPE_PROCESS));
  fprintf(log_file, "\n]\n");
  fflush(log_file);
}

static void handle_message(pony_msg_t* msg)
{
  // TODO: reduce duplication maybe so the same change doesn't have to be made in a 1000 places
  switch(msg->id)
  {
    case TRACING_MSG_THREAD_START:
    {
      tracing_thread_start_t* m = (tracing_thread_start_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"thread_start\",\"ts\":%" PRIu64 ",\"cat\":\"%s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"s\":\"%s\",\"args\":{\"index\":%d}}", convert_time_nanos_to_micros(m->ts), get_tracing_category_string(TRACING_CATEGORY_SCHEDULER), get_tracing_event_string(TRACING_EVENT_INSTANT), pid, m->thread_id, get_tracing_event_instant_scope_string(TRACING_EVENT_INSTANT_SCOPE_THREAD), m->index);
      fprintf(log_file, ",\n{\"name\":\"thread_name\",\"ts\":%" PRIu64 ",\"cat\":\"%s\",\"ph\":\"M\",\"pid\":%d,\"tid\":%" PRIu64 ",\"args\":{\"name\":\"%s: %d\"}}", convert_time_nanos_to_micros(m->ts), get_tracing_category_string(TRACING_CATEGORY_SCHEDULER), pid, m->thread_id, get_thread_name(m->index), m->index);
      fprintf(log_file, ",\n{\"name\":\"thread_sort_index\",\"ts\":%" PRIu64 ",\"cat\":\"%s\",\"ph\":\"M\",\"pid\":%d,\"tid\":%" PRIu64 ",\"args\":{\"sort_index\":%d}}", convert_time_nanos_to_micros(m->ts), get_tracing_category_string(TRACING_CATEGORY_SCHEDULER), pid, m->thread_id, m->index);
      break;
    }
    case TRACING_MSG_THREAD_STOP:
    {
      tracing_thread_stop_t* m = (tracing_thread_stop_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"thread_stop\",\"ts\":%" PRIu64 ",\"cat\":\"%s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"s\":\"%s\",\"args\":{\"index\":%d}}", convert_time_nanos_to_micros(m->ts), get_tracing_category_string(TRACING_CATEGORY_SCHEDULER), get_tracing_event_string(TRACING_EVENT_INSTANT), pid, m->thread_id, get_tracing_event_instant_scope_string(TRACING_EVENT_INSTANT_SCOPE_THREAD), m->index);

#if defined(USE_RUNTIMESTATS)
      fprintf(log_file, ",\n{\"name\":\"SCHEDULER\",\"ts\":%" PRIu64 ",\"cat\":\"scheduler stats\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%d\",\"args\":{\"total_memory_allocated\":%" PRIu64 ",\"total_memory_used\":%" PRIu64 ",\"created_actors_counter\":%lu,\"destroyed_actors_counter\":%lu,\"actors_app_cpu\":%lu,\"actors_gc_marking_cpu\":%lu,\"actors_gc_sweeping_cpu\":%lu,\"actors_system_cpu\":%lu,\"scheduler_msgs_cpu\":%lu,\"scheduler_misc_cpu\":%lu,\"memory_used_inflight_messages\":%" PRIu64 ",\"memory_allocated_inflight_messages\":%" PRIu64 ",\"number_of_inflight_messages\":%" PRIu64 "}}", convert_time_nanos_to_micros(m->ts), get_tracing_event_string(TRACING_EVENT_COUNTER), pid, m->thread_id, m->index,
        m->schedulerstats.mem_used + m->schedulerstats.mem_used_actors,
        m->schedulerstats.mem_allocated + m->schedulerstats.mem_allocated_actors,
        m->schedulerstats.created_actors_counter,
        m->schedulerstats.destroyed_actors_counter,
        m->schedulerstats.actor_app_cpu,
        m->schedulerstats.actor_gc_mark_cpu,
        m->schedulerstats.actor_gc_sweep_cpu,
        m->schedulerstats.actor_system_cpu,
        m->schedulerstats.msg_cpu,
        m->schedulerstats.misc_cpu,
#ifdef USE_RUNTIMESTATS_MESSAGES
        m->schedulerstats.mem_used_inflight_messages,
        m->schedulerstats.mem_allocated_inflight_messages,
        m->schedulerstats.num_inflight_messages
#else
        (int64_t)0,
        (int64_t)0,
        (int64_t)0
#endif
        );
#endif

      break;
    }
    case TRACING_MSG_THREAD_SUSPEND:
    {
      tracing_thread_suspend_t* m = (tracing_thread_suspend_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"thread_suspend\",\"ts\":%" PRIu64 ",\"cat\":\"%s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"s\":\"%s\",\"args\":{\"index\":%d}}", convert_time_nanos_to_micros(m->ts), get_tracing_category_string(TRACING_CATEGORY_SCHEDULER), get_tracing_event_string(TRACING_EVENT_INSTANT), pid, m->thread_id, get_tracing_event_instant_scope_string(TRACING_EVENT_INSTANT_SCOPE_THREAD), m->index);
      break;
    }
    case TRACING_MSG_THREAD_RESUME:
    {
      tracing_thread_resume_t* m = (tracing_thread_resume_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"thread_resume\",\"ts\":%" PRIu64 ",\"cat\":\"%s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"s\":\"%s\",\"args\":{\"index\":%d}}", convert_time_nanos_to_micros(m->ts), get_tracing_category_string(TRACING_CATEGORY_SCHEDULER), get_tracing_event_string(TRACING_EVENT_INSTANT), pid, m->thread_id, get_tracing_event_instant_scope_string(TRACING_EVENT_INSTANT_SCOPE_THREAD), m->index);

      #if defined(USE_RUNTIMESTATS)
      fprintf(log_file, ",\n{\"name\":\"SCHEDULER\",\"ts\":%" PRIu64 ",\"cat\":\"scheduler stats\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%d\",\"args\":{\"total_memory_allocated\":%" PRIu64 ",\"total_memory_used\":%" PRIu64 ",\"created_actors_counter\":%lu,\"destroyed_actors_counter\":%lu,\"actors_app_cpu\":%lu,\"actors_gc_marking_cpu\":%lu,\"actors_gc_sweeping_cpu\":%lu,\"actors_system_cpu\":%lu,\"scheduler_msgs_cpu\":%lu,\"scheduler_misc_cpu\":%lu,\"memory_used_inflight_messages\":%" PRIu64 ",\"memory_allocated_inflight_messages\":%" PRIu64 ",\"number_of_inflight_messages\":%" PRIu64 "}}", convert_time_nanos_to_micros(m->ts), get_tracing_event_string(TRACING_EVENT_COUNTER), pid, m->thread_id, m->index,
        m->schedulerstats.mem_used + m->schedulerstats.mem_used_actors,
        m->schedulerstats.mem_allocated + m->schedulerstats.mem_allocated_actors,
        m->schedulerstats.created_actors_counter,
        m->schedulerstats.destroyed_actors_counter,
        m->schedulerstats.actor_app_cpu,
        m->schedulerstats.actor_gc_mark_cpu,
        m->schedulerstats.actor_gc_sweep_cpu,
        m->schedulerstats.actor_system_cpu,
        m->schedulerstats.msg_cpu,
        m->schedulerstats.misc_cpu,
#ifdef USE_RUNTIMESTATS_MESSAGES
        m->schedulerstats.mem_used_inflight_messages,
        m->schedulerstats.mem_allocated_inflight_messages,
        m->schedulerstats.num_inflight_messages
#else
        (int64_t)0,
        (int64_t)0,
        (int64_t)0
#endif
        );
#endif

      break;
    }
    case TRACING_MSG_THREAD_RECEIVE_MESSAGE:
    {
      tracing_thread_receive_message_t* m = (tracing_thread_receive_message_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"RECEIVE:%s\",\"ts\":%" PRIu64 ",\"cat\":\"%s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"s\":\"%s\",\"args\":{\"arg\":%ld,\"index\":%d}}", get_scheduler_message_type_string(m->msg_type), convert_time_nanos_to_micros(m->ts), get_tracing_category_string(TRACING_CATEGORY_SCHEDULER_MESSAGING), get_tracing_event_string(TRACING_EVENT_INSTANT), pid, m->thread_id, get_tracing_event_instant_scope_string(TRACING_EVENT_INSTANT_SCOPE_THREAD), m->arg, m->index);
      break;
    }
    case TRACING_MSG_THREAD_SEND_MESSAGE:
    {
      tracing_thread_send_message_t* m = (tracing_thread_send_message_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"SEND:%s\",\"ts\":%" PRIu64 ",\"cat\":\"%s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"s\":\"%s\",\"args\":{\"arg\":%ld,\"index\":%d,\"to_index\":%d}}", get_scheduler_message_type_string(m->msg_type), convert_time_nanos_to_micros(m->ts), get_tracing_category_string(TRACING_CATEGORY_SCHEDULER_MESSAGING), get_tracing_event_string(TRACING_EVENT_INSTANT), pid, m->thread_id, get_tracing_event_instant_scope_string(TRACING_EVENT_INSTANT_SCOPE_THREAD), m->arg, m->index, m->to_sched_index);
      break;
    }
    case TRACING_MSG_ACTOR_CREATED:
    {
      tracing_actor_created_t* m = (tracing_actor_created_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"ACTOR: %s\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"id\":\"%p\",\"type_id\":%u,\"type_name\":\"%s\",\"blocked\":%s,\"blocked_sent\":%s,\"system\":%s,\"unscheduled\":%s,\"cd_contacted\":%s,\"rc_over_zero_seen\":%s,\"pinned\":%s,\"pending_destroy\":%s,\"overloaded\":%s,\"under_pressure\":%s,\"muted\":%s}}", m->type->name, convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_ASYNC_INSTANT), pid, m->thread_id, m->actor, m->actor, m->type->id, m->type->name,
        m->internal_flags & ACTOR_FLAG_BLOCKED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_BLOCKED_SENT ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_SYSTEM ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_UNSCHEDULED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_CD_CONTACTED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_RC_OVER_ZERO_SEEN ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_PINNED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_PENDINGDESTROY ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_OVERLOADED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_UNDER_PRESSURE ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_MUTED ? "true" : "false");
      break;
    }
    case TRACING_MSG_ACTOR_DESTROYED:
    {
      tracing_actor_destroyed_t* m = (tracing_actor_destroyed_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"%s destroyed\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"id\":\"%p\",\"type_id\":%u,\"type_name\":\"%s\",\"blocked\":%s,\"blocked_sent\":%s,\"system\":%s,\"unscheduled\":%s,\"cd_contacted\":%s,\"rc_over_zero_seen\":%s,\"pinned\":%s,\"pending_destroy\":%s,\"overloaded\":%s,\"under_pressure\":%s,\"muted\":%s,\"heap_next_gc\":%lu,\"heap_used\":%lu,\"gc_rc\":%lu,\"live_asio_events\":%u}}", m->type->name, convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_ASYNC_INSTANT), pid, m->thread_id, m->actor, m->actor, m->type->id, m->type->name,
        m->internal_flags & ACTOR_FLAG_BLOCKED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_BLOCKED_SENT ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_SYSTEM ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_UNSCHEDULED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_CD_CONTACTED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_RC_OVER_ZERO_SEEN ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_PINNED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_PENDINGDESTROY ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_OVERLOADED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_UNDER_PRESSURE ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_MUTED ? "true" : "false",
        m->heap_next_gc,
        m->heap_used,
        m->gc_rc,
        m->live_asio_events);

#if defined(USE_RUNTIMESTATS)
      fprintf(log_file, ",\n{\"name\":\"ACTOR: %s\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s stats\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"heap_next_gc\":%lu,\"heap_used\":%lu,\"gc_rc\":%lu,\"heap_memory_allocated\":%ld,\"heap_memory_used\":%ld,\"heap_num_allocated\":%ld,\"heap_realloc_counter\":%ld,\"heap_alloc_counter\":%ld,\"heap_free_counter\":%ld,\"heap_gc_counter\":%ld,\"system_cpu\":%ld,\"app_cpu\":%ld,\"garbage_collection_marking_cpu\":%ld,\"garbage_collection_sweeping_cpu\":%ld,\"messages_sent_counter\":%ld,\"system_messages_processed_counter\":%ld,\"app_messages_processed_counter\":%ld}}", m->type->name, convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_COUNTER), pid, m->thread_id, m->actor,
        m->heap_next_gc,
        m->heap_used,
        m->gc_rc,
        m->actorstats.heap_mem_allocated,
        m->actorstats.heap_mem_used,
        m->actorstats.heap_num_allocated,
        m->actorstats.heap_realloc_counter,
        m->actorstats.heap_alloc_counter,
        m->actorstats.heap_free_counter,
        m->actorstats.heap_gc_counter,
        m->actorstats.system_cpu,
        m->actorstats.app_cpu,
        m->actorstats.gc_mark_cpu,
        m->actorstats.gc_sweep_cpu,
        m->actorstats.messages_sent_counter,
        m->actorstats.system_messages_processed_counter,
        m->actorstats.app_messages_processed_counter);
#else
      fprintf(log_file, ",\n{\"name\":\"ACTOR: %s\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s stats\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"heap_next_gc\":%lu,\"heap_used\":%lu,\"gc_rc\":%lu}}", m->type->name, convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_COUNTER), pid, m->thread_id, m->actor,
        m->heap_next_gc,
        m->heap_used,
        m->gc_rc);
#endif

      break;
    }
    case TRACING_MSG_ACTOR_RUN_START:
    {
      tracing_actor_run_start_t* m = (tracing_actor_run_start_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"actor\",\"ts\":%" PRIu64 ",\"cat\":\"%s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"id\":\"%p\",\"type_id\":%u,\"type_name\":\"%s\",\"blocked\":%s,\"blocked_sent\":%s,\"system\":%s,\"unscheduled\":%s,\"cd_contacted\":%s,\"rc_over_zero_seen\":%s,\"pinned\":%s,\"pending_destroy\":%s,\"overloaded\":%s,\"under_pressure\":%s,\"muted\":%s}}", convert_time_nanos_to_micros(m->ts), get_tracing_category_string(TRACING_CATEGORY_ACTOR), get_tracing_event_string(TRACING_EVENT_CONTEXT_ENTER), pid, m->thread_id, m->actor, m->actor, m->type->id, m->type->name,
        m->internal_flags & ACTOR_FLAG_BLOCKED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_BLOCKED_SENT ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_SYSTEM ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_UNSCHEDULED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_CD_CONTACTED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_RC_OVER_ZERO_SEEN ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_PINNED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_PENDINGDESTROY ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_OVERLOADED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_UNDER_PRESSURE ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_MUTED ? "true" : "false");

      fprintf(log_file, ",\n{\"name\":\"actor_run\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"id\":\"%p\",\"type_id\":%u,\"type_name\":\"%s\",\"blocked\":%s,\"blocked_sent\":%s,\"system\":%s,\"unscheduled\":%s,\"cd_contacted\":%s,\"rc_over_zero_seen\":%s,\"pinned\":%s,\"pending_destroy\":%s,\"overloaded\":%s,\"under_pressure\":%s,\"muted\":%s}}", convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_ASYNC_BEGIN), pid, m->thread_id, m->actor, m->actor, m->type->id, m->type->name,
        m->internal_flags & ACTOR_FLAG_BLOCKED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_BLOCKED_SENT ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_SYSTEM ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_UNSCHEDULED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_CD_CONTACTED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_RC_OVER_ZERO_SEEN ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_PINNED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_PENDINGDESTROY ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_OVERLOADED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_UNDER_PRESSURE ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_MUTED ? "true" : "false");

      break;
    }
    case TRACING_MSG_ACTOR_RUN_STOP:
    {
      tracing_actor_run_stop_t* m = (tracing_actor_run_stop_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"actor_run\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"id\":\"%p\",\"type_id\":%u,\"type_name\":\"%s\",\"blocked\":%s,\"blocked_sent\":%s,\"system\":%s,\"unscheduled\":%s,\"cd_contacted\":%s,\"rc_over_zero_seen\":%s,\"pinned\":%s,\"pending_destroy\":%s,\"overloaded\":%s,\"under_pressure\":%s,\"muted\":%s,\"heap_next_gc\":%lu,\"heap_used\":%lu,\"gc_rc\":%lu,\"live_asio_events\":%u}}", convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_ASYNC_END), pid, m->thread_id, m->actor, m->actor, m->type->id, m->type->name,
        m->internal_flags & ACTOR_FLAG_BLOCKED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_BLOCKED_SENT ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_SYSTEM ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_UNSCHEDULED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_CD_CONTACTED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_RC_OVER_ZERO_SEEN ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_PINNED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_PENDINGDESTROY ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_OVERLOADED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_UNDER_PRESSURE ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_MUTED ? "true" : "false",
        m->heap_next_gc,
        m->heap_used,
        m->gc_rc,
        m->live_asio_events);

      fprintf(log_file, ",\n{\"name\":\"actor\",\"ts\":%" PRIu64 ",\"cat\":\"%s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"id\":\"%p\",\"type_id\":%u,\"type_name\":\"%s\",\"blocked\":%s,\"blocked_sent\":%s,\"system\":%s,\"unscheduled\":%s,\"cd_contacted\":%s,\"rc_over_zero_seen\":%s,\"pinned\":%s,\"pending_destroy\":%s,\"overloaded\":%s,\"under_pressure\":%s,\"muted\":%s,\"heap_next_gc\":%lu,\"heap_used\":%lu,\"gc_rc\":%lu,\"live_asio_events\":%u}}", convert_time_nanos_to_micros(m->ts), get_tracing_category_string(TRACING_CATEGORY_ACTOR), get_tracing_event_string(TRACING_EVENT_CONTEXT_LEAVE), pid, m->thread_id, m->actor, m->actor, m->type->id, m->type->name,
        m->internal_flags & ACTOR_FLAG_BLOCKED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_BLOCKED_SENT ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_SYSTEM ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_UNSCHEDULED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_CD_CONTACTED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_RC_OVER_ZERO_SEEN ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_PINNED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_PENDINGDESTROY ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_OVERLOADED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_UNDER_PRESSURE ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_MUTED ? "true" : "false",
        m->heap_next_gc,
        m->heap_used,
        m->gc_rc,
        m->live_asio_events);

#if defined(USE_RUNTIMESTATS)
      fprintf(log_file, ",\n{\"name\":\"ACTOR: %s\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s stats\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"heap_next_gc\":%lu,\"heap_used\":%lu,\"gc_rc\":%lu,\"heap_memory_allocated\":%ld,\"heap_memory_used\":%ld,\"heap_num_allocated\":%ld,\"heap_realloc_counter\":%ld,\"heap_alloc_counter\":%ld,\"heap_free_counter\":%ld,\"heap_gc_counter\":%ld,\"system_cpu\":%ld,\"app_cpu\":%ld,\"garbage_collection_marking_cpu\":%ld,\"garbage_collection_sweeping_cpu\":%ld,\"messages_sent_counter\":%ld,\"system_messages_processed_counter\":%ld,\"app_messages_processed_counter\":%ld}}", m->type->name, convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_COUNTER), pid, m->thread_id, m->actor,
        m->heap_next_gc,
        m->heap_used,
        m->gc_rc,
        m->actorstats.heap_mem_allocated,
        m->actorstats.heap_mem_used,
        m->actorstats.heap_num_allocated,
        m->actorstats.heap_realloc_counter,
        m->actorstats.heap_alloc_counter,
        m->actorstats.heap_free_counter,
        m->actorstats.heap_gc_counter,
        m->actorstats.system_cpu,
        m->actorstats.app_cpu,
        m->actorstats.gc_mark_cpu,
        m->actorstats.gc_sweep_cpu,
        m->actorstats.messages_sent_counter,
        m->actorstats.system_messages_processed_counter,
        m->actorstats.app_messages_processed_counter);
#else
      fprintf(log_file, ",\n{\"name\":\"ACTOR: %s\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s stats\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"heap_next_gc\":%lu,\"heap_used\":%lu,\"gc_rc\":%lu}}", m->type->name, convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_COUNTER), pid, m->thread_id, m->actor,
        m->heap_next_gc,
        m->heap_used,
        m->gc_rc);
#endif

      break;
    }
    case TRACING_MSG_ACTOR_BEHAVIOR_RUN_START:
    {
      tracing_actor_behavior_run_start_t* m = (tracing_actor_behavior_run_start_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"actor_behavior_run\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"id\":\"%p\",\"behavior_id\":%u,\"behavior_name\":\"%s\",\"type_id\":%u,\"type_name\":\"%s\",\"blocked\":%s,\"blocked_sent\":%s,\"system\":%s,\"unscheduled\":%s,\"cd_contacted\":%s,\"rc_over_zero_seen\":%s,\"pinned\":%s,\"pending_destroy\":%s,\"overloaded\":%s,\"under_pressure\":%s,\"muted\":%s}}", convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_ASYNC_BEGIN), pid, m->thread_id, m->actor, m->actor, m->behavior_id, get_actor_behavior_string(m->type->get_behavior_name, m->behavior_id), m->type->id, m->type->name,
        m->internal_flags & ACTOR_FLAG_BLOCKED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_BLOCKED_SENT ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_SYSTEM ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_UNSCHEDULED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_CD_CONTACTED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_RC_OVER_ZERO_SEEN ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_PINNED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_PENDINGDESTROY ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_OVERLOADED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_UNDER_PRESSURE ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_MUTED ? "true" : "false");

      break;
    }
    case TRACING_MSG_ACTOR_BEHAVIOR_RUN_STOP:
    {
      tracing_actor_behavior_run_end_t* m = (tracing_actor_behavior_run_end_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"actor_behavior_run\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"id\":\"%p\",\"behavior_id\":%u,\"behavior_name\":\"%s\",\"type_id\":%u,\"type_name\":\"%s\",\"blocked\":%s,\"blocked_sent\":%s,\"system\":%s,\"unscheduled\":%s,\"cd_contacted\":%s,\"rc_over_zero_seen\":%s,\"pinned\":%s,\"pending_destroy\":%s,\"overloaded\":%s,\"under_pressure\":%s,\"muted\":%s}}", convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_ASYNC_END), pid, m->thread_id, m->actor, m->actor, m->behavior_id, get_actor_behavior_string(m->type->get_behavior_name, m->behavior_id), m->type->id, m->type->name,
        m->internal_flags & ACTOR_FLAG_BLOCKED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_BLOCKED_SENT ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_SYSTEM ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_UNSCHEDULED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_CD_CONTACTED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_RC_OVER_ZERO_SEEN ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_PINNED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_PENDINGDESTROY ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_OVERLOADED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_UNDER_PRESSURE ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_MUTED ? "true" : "false");

      break;
    }
    case TRACING_MSG_ACTOR_GC_MARK_START:
    {
      tracing_actor_gc_mark_start_t* m = (tracing_actor_gc_mark_start_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"actor_gc_mark\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"id\":\"%p\",\"type_id\":%u,\"type_name\":\"%s\"}}", convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_ASYNC_BEGIN), pid, m->thread_id, m->actor, m->actor, m->type->id, m->type->name);
      break;
    }
    case TRACING_MSG_ACTOR_GC_MARK_END:
    {
      tracing_actor_gc_mark_end_t* m = (tracing_actor_gc_mark_end_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"actor_gc_mark\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"id\":\"%p\",\"type_id\":%u,\"type_name\":\"%s\"}}", convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_ASYNC_END), pid, m->thread_id, m->actor, m->actor, m->type->id, m->type->name);
      break;
    }
    case TRACING_MSG_ACTOR_GC_SWEEP_START:
    {
      tracing_actor_gc_sweep_start_t* m = (tracing_actor_gc_sweep_start_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"actor_gc_sweep\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"id\":\"%p\",\"type_id\":%u,\"type_name\":\"%s\"}}", convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_ASYNC_BEGIN), pid, m->thread_id, m->actor, m->actor, m->type->id, m->type->name);
      break;
    }
    case TRACING_MSG_ACTOR_GC_SWEEP_END:
    {
      tracing_actor_gc_sweep_end_t* m = (tracing_actor_gc_sweep_end_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"actor_gc_sweep\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"id\":\"%p\",\"type_id\":%u,\"type_name\":\"%s\"}}", convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_ASYNC_END), pid, m->thread_id, m->actor, m->actor, m->type->id, m->type->name);
      break;
    }
    case TRACING_MSG_ACTOR_MUTED:
    {
      tracing_actor_muted_t* m = (tracing_actor_muted_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"actor_muted\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"id\":\"%p\",\"type_id\":%u,\"type_name\":\"%s\",\"blocked\":%s,\"blocked_sent\":%s,\"system\":%s,\"unscheduled\":%s,\"cd_contacted\":%s,\"rc_over_zero_seen\":%s,\"pinned\":%s,\"pending_destroy\":%s,\"overloaded\":%s,\"under_pressure\":%s,\"muted\":%s}}", convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_ASYNC_INSTANT), pid, m->thread_id, m->actor, m->actor, m->type->id, m->type->name,
        m->internal_flags & ACTOR_FLAG_BLOCKED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_BLOCKED_SENT ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_SYSTEM ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_UNSCHEDULED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_CD_CONTACTED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_RC_OVER_ZERO_SEEN ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_PINNED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_PENDINGDESTROY ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_OVERLOADED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_UNDER_PRESSURE ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_MUTED ? "true" : "false");

      break;
    }
    case TRACING_MSG_ACTOR_UNMUTED:
    {
      tracing_actor_unmuted_t* m = (tracing_actor_unmuted_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"actor_unmuted\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"id\":\"%p\",\"type_id\":%u,\"type_name\":\"%s\",\"blocked\":%s,\"blocked_sent\":%s,\"system\":%s,\"unscheduled\":%s,\"cd_contacted\":%s,\"rc_over_zero_seen\":%s,\"pinned\":%s,\"pending_destroy\":%s,\"overloaded\":%s,\"under_pressure\":%s,\"muted\":%s}}", convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_ASYNC_INSTANT), pid, m->thread_id, m->actor, m->actor, m->type->id, m->type->name,
        m->internal_flags & ACTOR_FLAG_BLOCKED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_BLOCKED_SENT ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_SYSTEM ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_UNSCHEDULED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_CD_CONTACTED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_RC_OVER_ZERO_SEEN ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_PINNED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_PENDINGDESTROY ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_OVERLOADED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_UNDER_PRESSURE ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_MUTED ? "true" : "false");

      break;
    }
    case TRACING_MSG_ACTOR_OVERLOADED:
    {
      tracing_actor_overloaded_t* m = (tracing_actor_overloaded_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"actor_overloaded\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"id\":\"%p\",\"type_id\":%u,\"type_name\":\"%s\",\"blocked\":%s,\"blocked_sent\":%s,\"system\":%s,\"unscheduled\":%s,\"cd_contacted\":%s,\"rc_over_zero_seen\":%s,\"pinned\":%s,\"pending_destroy\":%s,\"overloaded\":%s,\"under_pressure\":%s,\"muted\":%s}}", convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_ASYNC_INSTANT), pid, m->thread_id, m->actor, m->actor, m->type->id, m->type->name,
        m->internal_flags & ACTOR_FLAG_BLOCKED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_BLOCKED_SENT ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_SYSTEM ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_UNSCHEDULED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_CD_CONTACTED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_RC_OVER_ZERO_SEEN ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_PINNED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_PENDINGDESTROY ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_OVERLOADED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_UNDER_PRESSURE ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_MUTED ? "true" : "false");

      break;
    }
    case TRACING_MSG_ACTOR_NOTOVERLOADED:
    {
      tracing_actor_notoverloaded_t* m = (tracing_actor_notoverloaded_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"actor_notoverloaded\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"id\":\"%p\",\"type_id\":%u,\"type_name\":\"%s\",\"blocked\":%s,\"blocked_sent\":%s,\"system\":%s,\"unscheduled\":%s,\"cd_contacted\":%s,\"rc_over_zero_seen\":%s,\"pinned\":%s,\"pending_destroy\":%s,\"overloaded\":%s,\"under_pressure\":%s,\"muted\":%s}}", convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_ASYNC_INSTANT), pid, m->thread_id, m->actor, m->actor, m->type->id, m->type->name,
        m->internal_flags & ACTOR_FLAG_BLOCKED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_BLOCKED_SENT ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_SYSTEM ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_UNSCHEDULED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_CD_CONTACTED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_RC_OVER_ZERO_SEEN ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_PINNED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_PENDINGDESTROY ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_OVERLOADED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_UNDER_PRESSURE ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_MUTED ? "true" : "false");

      break;
    }
    case TRACING_MSG_ACTOR_UNDERPRESSURE:
    {
      tracing_actor_underpressure_t* m = (tracing_actor_underpressure_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"actor_underpressure\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"id\":\"%p\",\"type_id\":%u,\"type_name\":\"%s\",\"blocked\":%s,\"blocked_sent\":%s,\"system\":%s,\"unscheduled\":%s,\"cd_contacted\":%s,\"rc_over_zero_seen\":%s,\"pinned\":%s,\"pending_destroy\":%s,\"overloaded\":%s,\"under_pressure\":%s,\"muted\":%s}}", convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_ASYNC_INSTANT), pid, m->thread_id, m->actor, m->actor, m->type->id, m->type->name,
        m->internal_flags & ACTOR_FLAG_BLOCKED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_BLOCKED_SENT ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_SYSTEM ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_UNSCHEDULED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_CD_CONTACTED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_RC_OVER_ZERO_SEEN ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_PINNED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_PENDINGDESTROY ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_OVERLOADED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_UNDER_PRESSURE ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_MUTED ? "true" : "false");

      break;
    }
    case TRACING_MSG_ACTOR_NOTUNDERPRESSURE:
    {
      tracing_actor_notunderpressure_t* m = (tracing_actor_notunderpressure_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"actor_notunderpressure\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"id\":\"%p\",\"type_id\":%u,\"type_name\":\"%s\",\"blocked\":%s,\"blocked_sent\":%s,\"system\":%s,\"unscheduled\":%s,\"cd_contacted\":%s,\"rc_over_zero_seen\":%s,\"pinned\":%s,\"pending_destroy\":%s,\"overloaded\":%s,\"under_pressure\":%s,\"muted\":%s}}", convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_ASYNC_INSTANT), pid, m->thread_id, m->actor, m->actor, m->type->id, m->type->name,
        m->internal_flags & ACTOR_FLAG_BLOCKED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_BLOCKED_SENT ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_SYSTEM ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_UNSCHEDULED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_CD_CONTACTED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_RC_OVER_ZERO_SEEN ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_PINNED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_PENDINGDESTROY ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_OVERLOADED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_UNDER_PRESSURE ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_MUTED ? "true" : "false");

      break;
    }
    case TRACING_MSG_ACTOR_BLOCKED:
    {
      tracing_actor_blocked_t* m = (tracing_actor_blocked_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"actor_blocked\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"id\":\"%p\",\"type_id\":%u,\"type_name\":\"%s\",\"blocked\":%s,\"blocked_sent\":%s,\"system\":%s,\"unscheduled\":%s,\"cd_contacted\":%s,\"rc_over_zero_seen\":%s,\"pinned\":%s,\"pending_destroy\":%s,\"overloaded\":%s,\"under_pressure\":%s,\"muted\":%s}}", convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_ASYNC_INSTANT), pid, m->thread_id, m->actor, m->actor, m->type->id, m->type->name,
        m->internal_flags & ACTOR_FLAG_BLOCKED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_BLOCKED_SENT ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_SYSTEM ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_UNSCHEDULED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_CD_CONTACTED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_RC_OVER_ZERO_SEEN ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_PINNED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_PENDINGDESTROY ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_OVERLOADED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_UNDER_PRESSURE ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_MUTED ? "true" : "false");

      break;
    }
    case TRACING_MSG_ACTOR_UNBLOCKED:
    {
      tracing_actor_unblocked_t* m = (tracing_actor_unblocked_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"actor_unblocked\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"id\":\"%p\",\"type_id\":%u,\"type_name\":\"%s\",\"blocked\":%s,\"blocked_sent\":%s,\"system\":%s,\"unscheduled\":%s,\"cd_contacted\":%s,\"rc_over_zero_seen\":%s,\"pinned\":%s,\"pending_destroy\":%s,\"overloaded\":%s,\"under_pressure\":%s,\"muted\":%s}}", convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_ASYNC_INSTANT), pid, m->thread_id, m->actor, m->actor, m->type->id, m->type->name,
        m->internal_flags & ACTOR_FLAG_BLOCKED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_BLOCKED_SENT ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_SYSTEM ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_UNSCHEDULED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_CD_CONTACTED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_RC_OVER_ZERO_SEEN ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_PINNED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_PENDINGDESTROY ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_OVERLOADED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_UNDER_PRESSURE ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_MUTED ? "true" : "false");

      break;
    }
    case TRACING_MSG_ACTOR_TRACING_ENABLED:
    {
      tracing_actor_destroyed_t* m = (tracing_actor_destroyed_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"%s tracing enabled\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"id\":\"%p\",\"type_id\":%u,\"type_name\":\"%s\",\"blocked\":%s,\"blocked_sent\":%s,\"system\":%s,\"unscheduled\":%s,\"cd_contacted\":%s,\"rc_over_zero_seen\":%s,\"pinned\":%s,\"pending_destroy\":%s,\"overloaded\":%s,\"under_pressure\":%s,\"muted\":%s,\"heap_next_gc\":%lu,\"heap_used\":%lu,\"gc_rc\":%lu,\"live_asio_events\":%u}}", m->type->name, convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_ASYNC_INSTANT), pid, m->thread_id, m->actor, m->actor, m->type->id, m->type->name,
        m->internal_flags & ACTOR_FLAG_BLOCKED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_BLOCKED_SENT ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_SYSTEM ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_UNSCHEDULED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_CD_CONTACTED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_RC_OVER_ZERO_SEEN ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_PINNED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_PENDINGDESTROY ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_OVERLOADED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_UNDER_PRESSURE ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_MUTED ? "true" : "false",
        m->heap_next_gc,
        m->heap_used,
        m->gc_rc,
        m->live_asio_events);

#if defined(USE_RUNTIMESTATS)
      fprintf(log_file, ",\n{\"name\":\"ACTOR: %s\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s stats\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"heap_next_gc\":%lu,\"heap_used\":%lu,\"gc_rc\":%lu,\"heap_memory_allocated\":%ld,\"heap_memory_used\":%ld,\"heap_num_allocated\":%ld,\"heap_realloc_counter\":%ld,\"heap_alloc_counter\":%ld,\"heap_free_counter\":%ld,\"heap_gc_counter\":%ld,\"system_cpu\":%ld,\"app_cpu\":%ld,\"garbage_collection_marking_cpu\":%ld,\"garbage_collection_sweeping_cpu\":%ld,\"messages_sent_counter\":%ld,\"system_messages_processed_counter\":%ld,\"app_messages_processed_counter\":%ld}}", m->type->name, convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_COUNTER), pid, m->thread_id, m->actor,
        m->heap_next_gc,
        m->heap_used,
        m->gc_rc,
        m->actorstats.heap_mem_allocated,
        m->actorstats.heap_mem_used,
        m->actorstats.heap_num_allocated,
        m->actorstats.heap_realloc_counter,
        m->actorstats.heap_alloc_counter,
        m->actorstats.heap_free_counter,
        m->actorstats.heap_gc_counter,
        m->actorstats.system_cpu,
        m->actorstats.app_cpu,
        m->actorstats.gc_mark_cpu,
        m->actorstats.gc_sweep_cpu,
        m->actorstats.messages_sent_counter,
        m->actorstats.system_messages_processed_counter,
        m->actorstats.app_messages_processed_counter);
#else
      fprintf(log_file, ",\n{\"name\":\"ACTOR: %s\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s stats\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"heap_next_gc\":%lu,\"heap_used\":%lu,\"gc_rc\":%lu}}", m->type->name, convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_COUNTER), pid, m->thread_id, m->actor,
        m->heap_next_gc,
        m->heap_used,
        m->gc_rc);
#endif

      break;
    }
    case TRACING_MSG_ACTOR_TRACING_DISABLED:
    {
      tracing_actor_destroyed_t* m = (tracing_actor_destroyed_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"%s tracing disabled\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"id\":\"%p\",\"type_id\":%u,\"type_name\":\"%s\",\"blocked\":%s,\"blocked_sent\":%s,\"system\":%s,\"unscheduled\":%s,\"cd_contacted\":%s,\"rc_over_zero_seen\":%s,\"pinned\":%s,\"pending_destroy\":%s,\"overloaded\":%s,\"under_pressure\":%s,\"muted\":%s,\"heap_next_gc\":%lu,\"heap_used\":%lu,\"gc_rc\":%lu,\"live_asio_events\":%u}}", m->type->name, convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_ASYNC_INSTANT), pid, m->thread_id, m->actor, m->actor, m->type->id, m->type->name,
        m->internal_flags & ACTOR_FLAG_BLOCKED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_BLOCKED_SENT ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_SYSTEM ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_UNSCHEDULED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_CD_CONTACTED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_RC_OVER_ZERO_SEEN ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_PINNED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_PENDINGDESTROY ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_OVERLOADED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_UNDER_PRESSURE ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_MUTED ? "true" : "false",
        m->heap_next_gc,
        m->heap_used,
        m->gc_rc,
        m->live_asio_events);

#if defined(USE_RUNTIMESTATS)
      fprintf(log_file, ",\n{\"name\":\"ACTOR: %s\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s stats\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"heap_next_gc\":%lu,\"heap_used\":%lu,\"gc_rc\":%lu,\"heap_memory_allocated\":%ld,\"heap_memory_used\":%ld,\"heap_num_allocated\":%ld,\"heap_realloc_counter\":%ld,\"heap_alloc_counter\":%ld,\"heap_free_counter\":%ld,\"heap_gc_counter\":%ld,\"system_cpu\":%ld,\"app_cpu\":%ld,\"garbage_collection_marking_cpu\":%ld,\"garbage_collection_sweeping_cpu\":%ld,\"messages_sent_counter\":%ld,\"system_messages_processed_counter\":%ld,\"app_messages_processed_counter\":%ld}}", m->type->name, convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_COUNTER), pid, m->thread_id, m->actor,
        m->heap_next_gc,
        m->heap_used,
        m->gc_rc,
        m->actorstats.heap_mem_allocated,
        m->actorstats.heap_mem_used,
        m->actorstats.heap_num_allocated,
        m->actorstats.heap_realloc_counter,
        m->actorstats.heap_alloc_counter,
        m->actorstats.heap_free_counter,
        m->actorstats.heap_gc_counter,
        m->actorstats.system_cpu,
        m->actorstats.app_cpu,
        m->actorstats.gc_mark_cpu,
        m->actorstats.gc_sweep_cpu,
        m->actorstats.messages_sent_counter,
        m->actorstats.system_messages_processed_counter,
        m->actorstats.app_messages_processed_counter);
#else
      fprintf(log_file, ",\n{\"name\":\"ACTOR: %s\",\"ts\":%" PRIu64 ",\"cat\":\"ACTOR: %s stats\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"id\":\"%p\",\"args\":{\"heap_next_gc\":%lu,\"heap_used\":%lu,\"gc_rc\":%lu}}", m->type->name, convert_time_nanos_to_micros(m->ts), m->type->name, get_tracing_event_string(TRACING_EVENT_COUNTER), pid, m->thread_id, m->actor,
        m->heap_next_gc,
        m->heap_used,
        m->gc_rc);
#endif

      break;
    }
    case TRACING_MSG_SYSTEMATIC_TESTING_CONFIG:
    {
      tracing_systematic_testing_config_t* m = (tracing_systematic_testing_config_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"systematic_testing_config\",\"ts\":%" PRIu64 ",\"cat\":\"%s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",\"args\":{\"random_seed\":\"%" PRIu64 "\"}}", convert_time_nanos_to_micros(m->ts), get_tracing_category_string(TRACING_CATEGORY_SYSTEMATIC_TESTING), get_tracing_event_string(TRACING_EVENT_INSTANT), pid, m->thread_id, m->random_seed);
      break;
    }
    case TRACING_MSG_SYSTEMATIC_TESTING_STARTED:
    {
      tracing_systematic_testing_started_t* m = (tracing_systematic_testing_started_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"systematic_testing_started\",\"ts\":%" PRIu64 ",\"cat\":\"%s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 "}", convert_time_nanos_to_micros(m->ts), get_tracing_category_string(TRACING_CATEGORY_SYSTEMATIC_TESTING), get_tracing_event_string(TRACING_EVENT_INSTANT), pid, m->thread_id);
      break;
    }
    case TRACING_MSG_SYSTEMATIC_TESTING_FINISHED:
    {
      tracing_systematic_testing_finished_t* m = (tracing_systematic_testing_finished_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"systematic_testing_finished\",\"ts\":%" PRIu64 ",\"cat\":\"%s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 "}", convert_time_nanos_to_micros(m->ts), get_tracing_category_string(TRACING_CATEGORY_SYSTEMATIC_TESTING), get_tracing_event_string(TRACING_EVENT_INSTANT), pid, m->thread_id);
      break;
    }
    case TRACING_MSG_SYSTEMATIC_TESTING_WAITING_TO_START_BEGIN:
    {
      tracing_systematic_testing_waiting_to_start_begin_t* m = (tracing_systematic_testing_waiting_to_start_begin_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"systematic_testing_waiting_to_start\",\"ts\":%" PRIu64 ",\"cat\":\"%s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 "}", convert_time_nanos_to_micros(m->ts), get_tracing_category_string(TRACING_CATEGORY_SYSTEMATIC_TESTING_DETAILS), get_tracing_event_string(TRACING_EVENT_BEGIN), pid, m->thread_id);
      break;
    }
    case TRACING_MSG_SYSTEMATIC_TESTING_WAITING_TO_START_END:
    {
      tracing_systematic_testing_waiting_to_start_end_t* m = (tracing_systematic_testing_waiting_to_start_end_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"systematic_testing_waiting_to_start\",\"ts\":%" PRIu64 ",\"cat\":\"%s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 "}", convert_time_nanos_to_micros(m->ts), get_tracing_category_string(TRACING_CATEGORY_SYSTEMATIC_TESTING_DETAILS), get_tracing_event_string(TRACING_EVENT_END), pid, m->thread_id);
      break;
    }
    case TRACING_MSG_SYSTEMATIC_TESTING_TIMESLICE_BEGIN:
    {
      tracing_systematic_testing_timeslice_begin_t* m = (tracing_systematic_testing_timeslice_begin_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"systematic_testing_timeslice\",\"ts\":%" PRIu64 ",\"cat\":\"%s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 "}", convert_time_nanos_to_micros(m->ts), get_tracing_category_string(TRACING_CATEGORY_SYSTEMATIC_TESTING_DETAILS), get_tracing_event_string(TRACING_EVENT_BEGIN), pid, m->thread_id);
      break;
    }
    case TRACING_MSG_SYSTEMATIC_TESTING_TIMESLICE_END:
    {
      tracing_systematic_testing_timeslice_end_t* m = (tracing_systematic_testing_timeslice_end_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"systematic_testing_timeslice\",\"ts\":%" PRIu64 ",\"cat\":\"%s\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 "}", convert_time_nanos_to_micros(m->ts), get_tracing_category_string(TRACING_CATEGORY_SYSTEMATIC_TESTING_DETAILS), get_tracing_event_string(TRACING_EVENT_END), pid, m->thread_id);
      break;
    }
    case TRACING_MSG_FLIGHT_RECORDER_DUMP:
    {
      // TODO: should flight recorder events be dumped to a file based on cli
      // options like normal tracing? or is stderr only/always acceptable?
      log_file = stderr;

      print_header(0);


      for(uint32_t sched_idx = 0; sched_idx < total_sched_count; sched_idx++)
      {
        if(NULL == tracing_schedulers[sched_idx].flight_recorder_events)
          continue;

        // simulate thread start message so that the thread is "nicely" displayed in viz tools
        // note: it is possible that we'll end up with multiple thread start messages if there
        // is another one pending in the flight recorder events for this thread
        tracing_thread_start_t* thread_start_msg = (tracing_thread_start_t*)pony_alloc_msg(
          POOL_INDEX(sizeof(tracing_thread_start_t)), TRACING_MSG_THREAD_START);
      
        thread_start_msg->index = tracing_schedulers[sched_idx].sched->index;
        thread_start_msg->thread_id = tracing_schedulers[sched_idx].tid;
        thread_start_msg->ts = 0;

        handle_message((pony_msg_t*)thread_start_msg);

        // make sure to free the message
        ponyint_pool_free(thread_start_msg->index, thread_start_msg);

        // dump all events for this scheduler
        for(uint64_t event_idx = 0; event_idx < flight_recorder_max_events; event_idx++)
        {
          pony_msg_t* msg = tracing_schedulers[sched_idx].flight_recorder_events[event_idx];
    
          if(msg != NULL)
            handle_message(msg);
        }
      }
  
      tracing_flight_recorder_dump_t* m = (tracing_flight_recorder_dump_t*)msg;
      fprintf(log_file, ",\n{\"name\":\"fatal_signal\",\"ts\":%" PRIu64 ",\"cat\":\"tracing\",\"ph\":\"%s\",\"pid\":%d,\"tid\":%" PRIu64 ",args:{\"signal\":%d,\"signal_name\":\"%s\",\"scheduler_index\":%d,\"active_actor\":", convert_time_nanos_to_micros(m->ts), get_tracing_event_string(TRACING_EVENT_INSTANT), pid, m->thread_id, m->siginfo->si_signo, strsignal(m->siginfo->si_signo), m->index);

      if(m->actor != NULL)
      {
        fprintf(log_file, "{\"id\":\"%p\",\"type_id\":%u,\"type_name\":\"%s\",\"blocked\":%s,\"blocked_sent\":%s,\"system\":%s,\"unscheduled\":%s,\"cd_contacted\":%s,\"rc_over_zero_seen\":%s,\"pinned\":%s,\"pending_destroy\":%s,\"overloaded\":%s,\"under_pressure\":%s,\"muted\":%s,\"heap_next_gc\":%lu,\"heap_used\":%lu,\"gc_rc\":%lu,\"live_asio_events\":%u}", m->actor, m->type->id, m->type->name,
        m->internal_flags & ACTOR_FLAG_BLOCKED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_BLOCKED_SENT ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_SYSTEM ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_UNSCHEDULED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_CD_CONTACTED ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_RC_OVER_ZERO_SEEN ? "true" : "false",
        m->internal_flags & ACTOR_FLAG_PINNED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_PENDINGDESTROY ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_OVERLOADED ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_UNDER_PRESSURE ? "true" : "false",
        m->sync_flags & ACTOR_SYNC_FLAG_MUTED ? "true" : "false",
        m->heap_next_gc,
        m->heap_used,
        m->gc_rc,
        m->live_asio_events);
      } else {
        fprintf(log_file, "null");
      }

      char** strings = backtrace_symbols(m->stack_frames, m->num_stack_frames);

      fprintf(log_file, ",\"stack\":[");
      for(stack_depth_t i = 0; i < m->num_stack_frames; i++)
      {
        if(i != 0)
          fprintf(log_file, ",");
        fprintf(log_file, "\"%s\"", strings[i]);
      }
      fprintf(log_file, "]}}");

      print_footer(0);

      fprintf(stderr, "\nflight recorder events dumped to `stderr` above..\n\n");

#if defined(PLATFORM_IS_MACOSX)
      psignal(m->siginfo->si_signo, "\nFatal signal encountered");
#else
      psiginfo(m->siginfo, "\nFatal signal encountered");
#endif

      fprintf(stderr, "Backtrace:\n");
    
      for(stack_depth_t i = 0; i < m->num_stack_frames; i++)
        fprintf(stderr, "  %s\n", strings[i]);

      free(strings);

      if(strcmp(PONY_BUILD_CONFIG, "release") == 0)
      {
        fputs("This is an optimised version of ponyc: the backtrace may be "
          "imprecise or incorrect.\nUse a debug version to get more meaningful "
          "information.\n", stderr);
      }

      atomic_store_explicit(&flight_recorder_dump_completed, true, memory_order_relaxed);

      // sleep for a bunch to hang this thread until program exits
      sleep(100000);
      break;
    }
    default:
    {
      pony_assert(0);
      break;
    }
  }
}

static void handle_queue()
{
  pony_msg_t* msg;

  while((msg = ponyint_thread_messageq_pop(&tracing_thread.mq
#ifdef USE_DYNAMIC_TRACE
    , PONY_TRACING_THREAD_INDEX
#endif
    )) != NULL)
  {
    handle_message(msg);
  }
}

static DECLARE_THREAD_FN(run_thread)
{
  (void)arg;
  ponyint_cpu_affinity(tracing_thread.cpu);

#if !defined(PLATFORM_IS_WINDOWS)
  // Make sure we block signals related to scheduler sleeping/waking
  // so they queue up to avoid race conditions
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, PONY_TRACING_SLEEP_WAKE_SIGNAL);
  pthread_sigmask(SIG_BLOCK, &set, NULL);
#endif

  while(!atomic_load_explicit(&stop_tracing_thread, memory_order_relaxed))
  {
    handle_queue();
    if(ponyint_messageq_markempty(&tracing_thread.mq))
    {
      // message queue is empty, suspend until woken up
      perhaps_suspend_tracing_thread();
    }
  }

  // handle queue one last time just in case something was somehow missed
  handle_queue();

  ponyint_pool_thread_cleanup();

  return 0;
}

// TODO: make it so tracing can be intialized/enabled/disabled at runtime by the user program?
// TODO: make it possible for actors to emit "flow"/"async" tracing events via a "tracing package/api" (have to figure out appropriate memcpy stuff for this due to GC)?
// TODO: should it be possible to have both the flight recorder and normal tracing enabled at the same time?
void ponyint_tracing_init(char* format, char* output, char* enabled_categories_patterns, char* mode, size_t flight_recorder_events_size, bool handle_term_int, char* force_actor_tracing)
{
  output_format = format;
  output_file = output;
  tracing_mode = mode;
  pid = getpid();
  enabled_categories_bitmask = 0;
  flight_recorder_handle_term_int = handle_term_int;

  force_actor_tracing_enabled = strcmp(force_actor_tracing, "all") == 0;

  if(strcmp(enabled_categories_patterns, "") != 0)
  {
    char* token = strtok(enabled_categories_patterns, ",");
    while(token != NULL)
    {
      if(fnmatch(token, "actor", FNM_NOESCAPE) == 0)
        enabled_categories_bitmask |= (uint64_t)TRACING_CATEGORY_ACTOR;

      if(fnmatch(token, "actor_behavior", FNM_NOESCAPE) == 0)
        enabled_categories_bitmask |= (uint64_t)TRACING_CATEGORY_ACTOR_BEHAVIOR;

      if(fnmatch(token, "actor_gc", FNM_NOESCAPE) == 0)
        enabled_categories_bitmask |= (uint64_t)TRACING_CATEGORY_ACTOR_GC;

      if(fnmatch(token, "actor_state_change", FNM_NOESCAPE) == 0)
        enabled_categories_bitmask |= (uint64_t)TRACING_CATEGORY_ACTOR_STATE_CHANGE;

      if(fnmatch(token, "scheduler", FNM_NOESCAPE) == 0)
        enabled_categories_bitmask |= (uint64_t)TRACING_CATEGORY_SCHEDULER;

      if(fnmatch(token, "scheduler_messaging", FNM_NOESCAPE) == 0)
        enabled_categories_bitmask |= (uint64_t)TRACING_CATEGORY_SCHEDULER_MESSAGING;

      if(fnmatch(token, "systematic_testing", FNM_NOESCAPE) == 0)
        enabled_categories_bitmask |= (uint64_t)TRACING_CATEGORY_SYSTEMATIC_TESTING;

      if(fnmatch(token, "systematic_testing_details", FNM_NOESCAPE) == 0)
        enabled_categories_bitmask |= (uint64_t)TRACING_CATEGORY_SYSTEMATIC_TESTING_DETAILS;

      token = strtok(NULL, ",");
    }
  }

  flight_recorder_max_events = ponyint_next_pow2(flight_recorder_events_size);
  flight_recorder_enabled = strcmp(tracing_mode, "flight_recorder") == 0;

  atomic_store_explicit(&stop_tracing_thread, false, memory_order_relaxed);

  tracing_thread.index = PONY_TRACING_THREAD_INDEX;
  // gets replace later with cpu to pin tracing thread on
  tracing_thread.cpu = -1;

  ponyint_messageq_init(&tracing_thread.mq);
}

void ponyint_tracing_schedulers_init(uint32_t sched_count, uint32_t tracing_cpu)
{
  tracing_thread.cpu = tracing_cpu;

  // initialize thread tracking array (should be sched_count + 2 to account for asio and pinned actor threads)
  total_sched_count = sched_count + 2;
  tracing_schedulers = ponyint_pool_alloc_size(sizeof(tracing_scheduler_t) * total_sched_count);
  memset(tracing_schedulers, 0, sizeof(tracing_scheduler_t) * total_sched_count);
}

bool ponyint_tracing_start()
{
  // nothing to do if no categories were enabled
  if(!enabled_categories_bitmask)
    return true;

  // TODO: other formats beyond JSON (i.e. fuchsia, perfetto, pprof, others?)
  if(strcmp(output_format, "json") != 0)
  {
    printf("ERROR: invalid trace format: %s\n", output_format);
    return false;
  }

  if(flight_recorder_enabled)
  {
    if(!ponyint_thread_create(&tracing_thread.tid, run_thread, tracing_thread.cpu,
      NULL))
    {
      printf("ERROR: Unable to create tracing thread for flight recorder\n");
      return false;
    }

    return true;
  }

  if(strcmp(tracing_mode, "file") != 0)
  {
    printf("ERROR: invalid tracing mode: %s\n", tracing_mode);
    return false;
  }

  if(strcmp(output_file, "-") == 0)
    log_file = stdout;
  else if(strcmp(output_file, "~") == 0)
    log_file = stderr;
  else
    log_file = fopen(output_file, "w");

  if(log_file == NULL)
  {
    printf("ERROR: Unable to open trace file: %s\n", output_file);
    return false;
  }

  print_header(this_tracing_scheduler->tid);

  if(!ponyint_thread_create(&tracing_thread.tid, run_thread, tracing_thread.cpu,
    NULL))
  {
    printf("ERROR: Unable to create tracing thread\n");
    return false;
  }

  return true;
}

void ponyint_tracing_stop()
{
  atomic_store_explicit(&stop_tracing_thread, true, memory_order_relaxed);

  // nothing to do if no categories were enabled
  if(!enabled_categories_bitmask)
    return;

  // wake tracing thread if it was sleeping because its queue was empty
  wake_suspended_tracing_thread();

  while(!ponyint_thread_join(tracing_thread.tid))
  { }

  ponyint_messageq_destroy(&tracing_thread.mq, false);

  // need to free all the flight recorder events for all schedulers
  if(flight_recorder_enabled)
  {
    for(uint32_t sched_idx = 0; sched_idx < total_sched_count; sched_idx++)
    {
      if(NULL == tracing_schedulers[sched_idx].flight_recorder_events)
        continue;

      for(uint64_t event_idx = 0; event_idx < flight_recorder_max_events; event_idx++)
      {
        pony_msg_t* msg = tracing_schedulers[sched_idx].flight_recorder_events[event_idx];
  
        if(msg != NULL)
        {
          ponyint_pool_free(msg->index, msg);
          tracing_schedulers[sched_idx].flight_recorder_events[event_idx] = NULL;
        }
      }
  
      // and the flight recorder buffer
      ponyint_pool_free_size(sizeof(pony_msg_t*) * flight_recorder_max_events, tracing_schedulers[sched_idx].flight_recorder_events);
      tracing_schedulers[sched_idx].flight_recorder_events = NULL;
    }

    ponyint_pool_free_size(sizeof(tracing_scheduler_t) * total_sched_count, tracing_schedulers);
    tracing_schedulers = NULL;
    return;
  }

  print_footer(this_tracing_scheduler->tid);

  fclose(log_file);
  log_file = NULL;

  ponyint_pool_free_size(sizeof(tracing_scheduler_t) * total_sched_count, tracing_schedulers);
  tracing_schedulers = NULL;
}

#endif
