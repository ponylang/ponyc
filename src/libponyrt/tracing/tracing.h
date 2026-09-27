#ifndef tracing_tracing_h
#define tracing_tracing_h

#include <platform.h>
#include "../actor/actor.h"
#include "../sched/scheduler.h"

PONY_EXTERN_C_BEGIN

extern bool ponyint_tracing_enabled;

void ponyint_tracing_init(char* format, char* output, char* enabled_categories_patterns, char* mode, size_t flight_recorder_events_size, bool handle_term_int, char* force_actor_tracing);
void ponyint_tracing_schedulers_init(uint32_t sched_count, uint32_t tracing_cpu);
bool ponyint_tracing_start();
void ponyint_tracing_stop();
void ponyint_tracing_thread_start(scheduler_t* sched);
void ponyint_tracing_thread_stop();
void ponyint_tracing_thread_suspend();
void ponyint_tracing_thread_resume();
void ponyint_tracing_thread_receive_message(pony_msgi_t* msg_id, sched_msg_t msg_type, intptr_t arg);
void ponyint_tracing_thread_send_message(pony_msgi_t* msg_id, sched_msg_t msg_type, intptr_t arg, int32_t from_sched_index, int32_t to_sched_index);
void ponyint_tracing_thread_actor_run_start(pony_actor_t* actor);
void ponyint_tracing_thread_actor_run_stop(pony_actor_t* actor);
void ponyint_tracing_actor_created(pony_actor_t* actor);
void ponyint_tracing_actor_destroyed(pony_actor_t* actor, actor_destroyed_reason_t reason);
void ponyint_tracing_actor_behavior_run_schedule(pony_actor_t* actor, pony_actor_t* to_actor, pony_msg_t* msg_id, uint32_t behavior_id);
void ponyint_tracing_actor_behavior_run_start(pony_actor_t* actor, pony_msg_t* msg_id, uint32_t behavior_id);
void ponyint_tracing_actor_behavior_run_end(pony_actor_t* actor, pony_msg_t* msg_id, uint32_t behavior_id);
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
#define TRACING_THREAD_START(SCHED) do { if (ponyint_tracing_enabled) ponyint_tracing_thread_start(SCHED); } while(0)
#define TRACING_THREAD_STOP() do { if (ponyint_tracing_enabled) ponyint_tracing_thread_stop(); } while(0)
#define TRACING_THREAD_SUSPEND() do { if (ponyint_tracing_enabled) ponyint_tracing_thread_suspend(); } while(0)
#define TRACING_THREAD_RESUME() do { if (ponyint_tracing_enabled) ponyint_tracing_thread_resume(); } while(0)
#define TRACING_THREAD_RECEIVE_MESSAGE(MSG_ID, MSG_TYPE, ARG) do { if (ponyint_tracing_enabled) ponyint_tracing_thread_receive_message(MSG_ID, MSG_TYPE, ARG); } while(0)
#define TRACING_THREAD_SEND_MESSAGE(MSG_ID, MSG_TYPE, ARG, FROM_SCHED_INDEX, TO_SCHED_INDEX) do { if (ponyint_tracing_enabled) ponyint_tracing_thread_send_message(MSG_ID, MSG_TYPE, ARG, FROM_SCHED_INDEX, TO_SCHED_INDEX); } while(0)
#define TRACING_THREAD_ACTOR_RUN_START(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_thread_actor_run_start(ACTOR); } while(0)
#define TRACING_THREAD_ACTOR_RUN_STOP(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_thread_actor_run_stop(ACTOR); } while(0)
#define TRACING_ACTOR_CREATED(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_created(ACTOR); } while(0)
#define TRACING_ACTOR_DESTROYED(ACTOR, REASON) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_destroyed(ACTOR, REASON); } while(0)
#define TRACING_ACTOR_BEHAVIOR_RUN_SCHEDULE(ACTOR, TO_ACTOR, MSG_ID, BEHAVIOR_ID) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_behavior_run_schedule(ACTOR, TO_ACTOR, MSG_ID, BEHAVIOR_ID); } while(0)
#define TRACING_ACTOR_BEHAVIOR_RUN_START(ACTOR, MSG_ID, BEHAVIOR_ID) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_behavior_run_start(ACTOR, MSG_ID, BEHAVIOR_ID); } while(0)
#define TRACING_ACTOR_BEHAVIOR_RUN_END(ACTOR, MSG_ID, BEHAVIOR_ID) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_behavior_run_end(ACTOR, MSG_ID, BEHAVIOR_ID); } while(0)
#define TRACING_ACTOR_GC_START(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_gc_start(ACTOR); } while(0)
#define TRACING_ACTOR_GC_END(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_gc_end(ACTOR); } while(0)
#define TRACING_ACTOR_GC_MARK_START(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_gc_mark_start(ACTOR); } while(0)
#define TRACING_ACTOR_GC_MARK_END(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_gc_mark_end(ACTOR); } while(0)
#define TRACING_ACTOR_GC_SWEEP_START(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_gc_sweep_start(ACTOR); } while(0)
#define TRACING_ACTOR_GC_SWEEP_END(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_gc_sweep_end(ACTOR); } while(0)
#define TRACING_ACTOR_GC_OBJECTMAP_SWEEP_START(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_gc_objectmap_sweep_start(ACTOR); } while(0)
#define TRACING_ACTOR_GC_OBJECTMAP_SWEEP_END(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_gc_objectmap_sweep_end(ACTOR); } while(0)
#define TRACING_ACTOR_GC_ACTORMAP_SWEEP_START(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_gc_actormap_sweep_start(ACTOR); } while(0)
#define TRACING_ACTOR_GC_ACTORMAP_SWEEP_END(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_gc_actormap_sweep_end(ACTOR); } while(0)
#define TRACING_ACTOR_GC_HEAP_SWEEP_START(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_gc_heap_sweep_start(ACTOR); } while(0)
#define TRACING_ACTOR_GC_HEAP_SWEEP_END(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_gc_heap_sweep_end(ACTOR); } while(0)
#define TRACING_ACTOR_MUTED(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_muted(ACTOR); } while(0)
#define TRACING_ACTOR_UNMUTED(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_unmuted(ACTOR); } while(0)
#define TRACING_ACTOR_OVERLOADED(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_overloaded(ACTOR); } while(0)
#define TRACING_ACTOR_NOTOVERLOADED(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_notoverloaded(ACTOR); } while(0)
#define TRACING_ACTOR_UNDERPRESSURE(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_underpressure(ACTOR); } while(0)
#define TRACING_ACTOR_NOTUNDERPRESSURE(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_notunderpressure(ACTOR); } while(0)
#define TRACING_ACTOR_BLOCKED(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_blocked(ACTOR); } while(0)
#define TRACING_ACTOR_UNBLOCKED(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_unblocked(ACTOR); } while(0)
#define TRACING_ACTOR_TRACING_ENABLED(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_tracing_enabled(ACTOR); } while(0)
#define TRACING_ACTOR_TRACING_DISABLED(ACTOR) do { if (ponyint_tracing_enabled) ponyint_tracing_actor_tracing_disabled(ACTOR); } while(0)
#define TRACING_SYSTEMATIC_TESTING_CONFIG(RANDOM_SEED) do { if (ponyint_tracing_enabled) ponyint_tracing_systematic_testing_config(RANDOM_SEED); } while(0)
#define TRACING_SYSTEMATIC_TESTING_STARTED() do { if (ponyint_tracing_enabled) ponyint_tracing_systematic_testing_started(); } while(0)
#define TRACING_SYSTEMATIC_TESTING_FINISHED() do { if (ponyint_tracing_enabled) ponyint_tracing_systematic_testing_finished(); } while(0)
#define TRACING_SYSTEMATIC_TESTING_WAITING_TO_START_BEGIN() do { if (ponyint_tracing_enabled) ponyint_tracing_systematic_testing_waiting_to_start_begin(); } while(0)
#define TRACING_SYSTEMATIC_TESTING_WAITING_TO_START_END() do { if (ponyint_tracing_enabled) ponyint_tracing_systematic_testing_waiting_to_start_end(); } while(0)
#define TRACING_SYSTEMATIC_TESTING_TIMESLICE_BEGIN() do { if (ponyint_tracing_enabled) ponyint_tracing_systematic_testing_timeslice_begin(); } while(0)
#define TRACING_SYSTEMATIC_TESTING_TIMESLICE_END() do { if (ponyint_tracing_enabled) ponyint_tracing_systematic_testing_timeslice_end(); } while(0)

PONY_EXTERN_C_END

#endif
