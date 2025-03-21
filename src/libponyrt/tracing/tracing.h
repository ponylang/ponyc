#ifndef tracing_tracing_h
#define tracing_tracing_h

#include <platform.h>
#include "../actor/actor.h"
#include "../sched/scheduler.h"

#if defined(USE_RUNTIME_TRACING)
#if defined(PLATFORM_IS_WINDOWS)
pony_static_assert(false, "Runtime tracing doesn't support windows at the moment!");
#endif

PONY_EXTERN_C_BEGIN

void ponyint_tracing_init(char* format, char* output, char* enabled_categories_patterns, char* mode, size_t flight_recorder_events_size, bool handle_term_int, char* force_actor_tracing);
void ponyint_tracing_schedulers_init(uint32_t sched_count, uint32_t tracing_cpu);
bool ponyint_tracing_start();
void ponyint_tracing_stop();
void ponyint_tracing_thread_start(scheduler_t* sched);
void ponyint_tracing_thread_stop();
void ponyint_tracing_thread_suspend();
void ponyint_tracing_thread_resume();
void ponyint_tracing_thread_receive_message(sched_msg_t msg_type, intptr_t arg);
void ponyint_tracing_thread_send_message(sched_msg_t msg_type, intptr_t arg, int32_t from_sched_index, int32_t to_sched_index);
void ponyint_tracing_thread_actor_run_start(pony_actor_t* actor);
void ponyint_tracing_thread_actor_run_stop(pony_actor_t* actor);
void ponyint_tracing_actor_created(pony_actor_t* actor);
void ponyint_tracing_actor_destroyed(pony_actor_t* actor);
void ponyint_tracing_actor_behavior_run_start(pony_actor_t* actor, uint32_t behavior_id);
void ponyint_tracing_actor_behavior_run_end(pony_actor_t* actor, uint32_t behavior_id);
void ponyint_tracing_actor_gc_start(pony_actor_t* actor);
void ponyint_tracing_actor_gc_end(pony_actor_t* actor);
void ponyint_tracing_actor_gc_mark_start(pony_actor_t* actor);
void ponyint_tracing_actor_gc_mark_end(pony_actor_t* actor);
void ponyint_tracing_actor_gc_sweep_start(pony_actor_t* actor);
void ponyint_tracing_actor_gc_sweep_end(pony_actor_t* actor);
void ponyint_tracing_actor_gc_objectmap_sweep_start(pony_actor_t* actor);
void ponyint_tracing_actor_gc_objectmap_sweep_end(pony_actor_t* actor);
void ponyint_tracing_actor_gc_actormap_sweep_start(pony_actor_t* actor);
void ponyint_tracing_actor_gc_actormap_sweep_end(pony_actor_t* actor);
void ponyint_tracing_actor_gc_heap_sweep_start(pony_actor_t* actor);
void ponyint_tracing_actor_gc_heap_sweep_end(pony_actor_t* actor);
void ponyint_tracing_actor_muted(pony_actor_t* actor);
void ponyint_tracing_actor_unmuted(pony_actor_t* actor);
void ponyint_tracing_actor_overloaded(pony_actor_t* actor);
void ponyint_tracing_actor_notoverloaded(pony_actor_t* actor);
void ponyint_tracing_actor_underpressure(pony_actor_t* actor);
void ponyint_tracing_actor_notunderpressure(pony_actor_t* actor);
void ponyint_tracing_actor_blocked(pony_actor_t* actor);
void ponyint_tracing_actor_unblocked(pony_actor_t* actor);
void ponyint_tracing_actor_tracing_enabled(pony_actor_t* actor);
void ponyint_tracing_actor_tracing_disabled(pony_actor_t* actor);
void ponyint_tracing_systematic_testing_config(uint64_t random_seed);
void ponyint_tracing_systematic_testing_started();
void ponyint_tracing_systematic_testing_finished();
void ponyint_tracing_systematic_testing_waiting_to_start_begin();
void ponyint_tracing_systematic_testing_waiting_to_start_end();
void ponyint_tracing_systematic_testing_timeslice_begin();
void ponyint_tracing_systematic_testing_timeslice_end();


#define TRACING_INIT(FORMAT, OUTPUT, ENABLED_CATEGORIES_PATTERNS, MODE, FLIGHT_RECORDER_EVENTS_SIZE, HANDLE_TERM_INT, FORCE_ACTOR_TRACING) ponyint_tracing_init(FORMAT, OUTPUT, ENABLED_CATEGORIES_PATTERNS, MODE, FLIGHT_RECORDER_EVENTS_SIZE, HANDLE_TERM_INT, FORCE_ACTOR_TRACING)
#define TRACING_SCHEDULERS_INIT(SCHED_COUNT, TRACING_CPU) ponyint_tracing_schedulers_init(SCHED_COUNT, TRACING_CPU)
#define TRACING_START() ponyint_tracing_start()
#define TRACING_STOP() ponyint_tracing_stop()
#define TRACING_THREAD_START(SCHED) ponyint_tracing_thread_start(SCHED)
#define TRACING_THREAD_STOP() ponyint_tracing_thread_stop()
#define TRACING_THREAD_SUSPEND() ponyint_tracing_thread_suspend()
#define TRACING_THREAD_RESUME() ponyint_tracing_thread_resume()
#define TRACING_THREAD_RECEIVE_MESSAGE(MSG_TYPE, ARG) ponyint_tracing_thread_receive_message(MSG_TYPE, ARG)
#define TRACING_THREAD_SEND_MESSAGE(MSG_TYPE, ARG, FROM_SCHED_INDEX, TO_SCHED_INDEX) ponyint_tracing_thread_send_message(MSG_TYPE, ARG, FROM_SCHED_INDEX, TO_SCHED_INDEX)
#define TRACING_THREAD_ACTOR_RUN_START(ACTOR) ponyint_tracing_thread_actor_run_start(ACTOR)
#define TRACING_THREAD_ACTOR_RUN_STOP(ACTOR) ponyint_tracing_thread_actor_run_stop(ACTOR)
#define TRACING_ACTOR_CREATED(ACTOR) ponyint_tracing_actor_created(ACTOR)
#define TRACING_ACTOR_DESTROYED(ACTOR) ponyint_tracing_actor_destroyed(ACTOR)
#define TRACING_ACTOR_BEHAVIOR_RUN_START(ACTOR, BEHAVIOR_ID) ponyint_tracing_actor_behavior_run_start(ACTOR, BEHAVIOR_ID)
#define TRACING_ACTOR_BEHAVIOR_RUN_END(ACTOR, BEHAVIOR_ID) ponyint_tracing_actor_behavior_run_end(ACTOR, BEHAVIOR_ID)
#define TRACING_ACTOR_GC_START(ACTOR) ponyint_tracing_actor_gc_start(ACTOR)
#define TRACING_ACTOR_GC_END(ACTOR) ponyint_tracing_actor_gc_end(ACTOR)
#define TRACING_ACTOR_GC_MARK_START(ACTOR) ponyint_tracing_actor_gc_mark_start(ACTOR)
#define TRACING_ACTOR_GC_MARK_END(ACTOR) ponyint_tracing_actor_gc_mark_end(ACTOR)
#define TRACING_ACTOR_GC_SWEEP_START(ACTOR) ponyint_tracing_actor_gc_sweep_start(ACTOR)
#define TRACING_ACTOR_GC_SWEEP_END(ACTOR) ponyint_tracing_actor_gc_sweep_end(ACTOR)
#define TRACING_ACTOR_GC_OBJECTMAP_SWEEP_START(ACTOR) ponyint_tracing_actor_gc_objectmap_sweep_start(ACTOR)
#define TRACING_ACTOR_GC_OBJECTMAP_SWEEP_END(ACTOR) ponyint_tracing_actor_gc_objectmap_sweep_end(ACTOR)
#define TRACING_ACTOR_GC_ACTORMAP_SWEEP_START(ACTOR) ponyint_tracing_actor_gc_actormap_sweep_start(ACTOR)
#define TRACING_ACTOR_GC_ACTORMAP_SWEEP_END(ACTOR) ponyint_tracing_actor_gc_actormap_sweep_end(ACTOR)
#define TRACING_ACTOR_GC_HEAP_SWEEP_START(ACTOR) ponyint_tracing_actor_gc_heap_sweep_start(ACTOR)
#define TRACING_ACTOR_GC_HEAP_SWEEP_END(ACTOR) ponyint_tracing_actor_gc_heap_sweep_end(ACTOR)
#define TRACING_ACTOR_MUTED(ACTOR) ponyint_tracing_actor_muted(ACTOR)
#define TRACING_ACTOR_UNMUTED(ACTOR) ponyint_tracing_actor_unmuted(ACTOR)
#define TRACING_ACTOR_OVERLOADED(ACTOR) ponyint_tracing_actor_overloaded(ACTOR)
#define TRACING_ACTOR_NOTOVERLOADED(ACTOR) ponyint_tracing_actor_notoverloaded(ACTOR)
#define TRACING_ACTOR_UNDERPRESSURE(ACTOR) ponyint_tracing_actor_underpressure(ACTOR)
#define TRACING_ACTOR_NOTUNDERPRESSURE(ACTOR) ponyint_tracing_actor_notunderpressure(ACTOR)
#define TRACING_ACTOR_BLOCKED(ACTOR) ponyint_tracing_actor_blocked(ACTOR)
#define TRACING_ACTOR_UNBLOCKED(ACTOR) ponyint_tracing_actor_unblocked(ACTOR)
#define TRACING_ACTOR_TRACING_ENABLED(ACTOR) ponyint_tracing_actor_tracing_enabled(ACTOR)
#define TRACING_ACTOR_TRACING_DISABLED(ACTOR) ponyint_tracing_actor_tracing_disabled(ACTOR)
#define TRACING_SYSTEMATIC_TESTING_CONFIG(RANDOM_SEED) ponyint_tracing_systematic_testing_config(RANDOM_SEED)
#define TRACING_SYSTEMATIC_TESTING_STARTED() ponyint_tracing_systematic_testing_started()
#define TRACING_SYSTEMATIC_TESTING_FINISHED() ponyint_tracing_systematic_testing_finished()
#define TRACING_SYSTEMATIC_TESTING_WAITING_TO_START_BEGIN() ponyint_tracing_systematic_testing_waiting_to_start_begin()
#define TRACING_SYSTEMATIC_TESTING_WAITING_TO_START_END() ponyint_tracing_systematic_testing_waiting_to_start_end()
#define TRACING_SYSTEMATIC_TESTING_TIMESLICE_BEGIN() ponyint_tracing_systematic_testing_timeslice_begin()
#define TRACING_SYSTEMATIC_TESTING_TIMESLICE_END() ponyint_tracing_systematic_testing_timeslice_end()

PONY_EXTERN_C_END

#else

#define TRACING_INIT(FORMAT, OUTPUT, ENABLED_CATEGORIES_PATTERNS, MODE, FLIGHT_RECORDER_EVENTS_SIZE, HANDLE_TERM_INT, FORCE_ACTOR_TRACING)
#define TRACING_SCHEDULERS_INIT(SCHED_COUNT, TRACING_CPU)
#define TRACING_START() true
#define TRACING_STOP()
#define TRACING_THREAD_START(SCHED)
#define TRACING_THREAD_STOP()
#define TRACING_THREAD_SUSPEND()
#define TRACING_THREAD_RESUME()
#define TRACING_THREAD_RECEIVE_MESSAGE(MSG_TYPE, ARG)
#define TRACING_THREAD_SEND_MESSAGE(MSG_TYPE, ARG, FROM_SCHED_INDEX, TO_SCHED_INDEX)
#define TRACING_THREAD_ACTOR_RUN_START(ACTOR)
#define TRACING_THREAD_ACTOR_RUN_STOP(ACTOR)
#define TRACING_ACTOR_CREATED(ACTOR)
#define TRACING_ACTOR_DESTROYED(ACTOR)
#define TRACING_ACTOR_BEHAVIOR_RUN_START(ACTOR, BEHAVIOR_ID)
#define TRACING_ACTOR_BEHAVIOR_RUN_END(ACTOR, BEHAVIOR_ID)
#define TRACING_ACTOR_GC_START(ACTOR)
#define TRACING_ACTOR_GC_END(ACTOR)
#define TRACING_ACTOR_GC_MARK_START(ACTOR)
#define TRACING_ACTOR_GC_MARK_END(ACTOR)
#define TRACING_ACTOR_GC_SWEEP_START(ACTOR)
#define TRACING_ACTOR_GC_SWEEP_END(ACTOR)
#define TRACING_ACTOR_GC_OBJECTMAP_SWEEP_START(ACTOR)
#define TRACING_ACTOR_GC_OBJECTMAP_SWEEP_END(ACTOR)
#define TRACING_ACTOR_GC_ACTORMAP_SWEEP_START(ACTOR)
#define TRACING_ACTOR_GC_ACTORMAP_SWEEP_END(ACTOR)
#define TRACING_ACTOR_GC_HEAP_SWEEP_START(ACTOR)
#define TRACING_ACTOR_GC_HEAP_SWEEP_END(ACTOR)
#define TRACING_ACTOR_MUTED(ACTOR)
#define TRACING_ACTOR_UNMUTED(ACTOR)
#define TRACING_ACTOR_OVERLOADED(ACTOR)
#define TRACING_ACTOR_NOTOVERLOADED(ACTOR)
#define TRACING_ACTOR_UNDERPRESSURE(ACTOR)
#define TRACING_ACTOR_NOTUNDERPRESSURE(ACTOR)
#define TRACING_ACTOR_BLOCKED(ACTOR)
#define TRACING_ACTOR_UNBLOCKED(ACTOR)
#define TRACING_ACTOR_TRACING_ENABLED(ACTOR)
#define TRACING_ACTOR_TRACING_DISABLED(ACTOR)
#define TRACING_SYSTEMATIC_TESTING_CONFIG(RANDOM_SEED)
#define TRACING_SYSTEMATIC_TESTING_STARTED()
#define TRACING_SYSTEMATIC_TESTING_FINISHED()
#define TRACING_SYSTEMATIC_TESTING_WAITING_TO_START_BEGIN()
#define TRACING_SYSTEMATIC_TESTING_WAITING_TO_START_END()
#define TRACING_SYSTEMATIC_TESTING_TIMESLICE_BEGIN()
#define TRACING_SYSTEMATIC_TESTING_TIMESLICE_END()

#endif

#endif
