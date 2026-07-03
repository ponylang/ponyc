# Refusing I/O under systematic testing is what keeps get_next_index's divisor non-zero

Under `use=systematic_testing` the round-robin picker
`get_next_index` (`src/libponyrt/sched/systematic_testing.c`) chooses the next
thread with `rand() % active_count`, where `active_count` is the number of
active scheduler threads plus one when the pinned-actor thread is not suspended.
`active_count` must be `>= 1` or the modulo divides by zero.

`active_count` can only reach zero if every scheduler thread has suspended
(`active_scheduler_count == 0`) while the pinned-actor thread is also suspended.
Scheduler 0 is the last scheduler to suspend, and `perhaps_suspend_scheduler`
(`src/libponyrt/sched/scheduler.c`) only lets scheduler 0 suspend when it has a
noisy actor -- an actor with a registered ASIO event. Under systematic testing
no ASIO event can ever be registered: `pony_asio_event_create`
(`src/libponyrt/asio/event.c`) aborts. So scheduler 0 never suspends,
`active_scheduler_count` stays `>= 1`, and the divisor is safe.

The dependency is therefore: the I/O abort in `event.c` and the scheduler-0
suspend gate in `scheduler.c` are jointly what guarantee `get_next_index`'s
divisor is non-zero. Two changes would break it and reintroduce a divide-by-zero
that no normal run would surface: allowing I/O (removing or weakening the
`event.c` abort) so an actor can become noisy, or letting scheduler 0 suspend
without a noisy actor. `get_next_index` carries a `pony_assert(active_count > 0)`
so a debug build traps loudly at the point of breakage instead of dividing by
zero. Before the ASIO thread was removed from systematic testing it occupied a
round-robin slot that kept the count `>= 1` on its own; that slot is gone, so
this invariant now rests entirely on the two guards above.

That assert is defense-in-depth that no current test can fire: reaching
`active_count == 0` requires a noisy actor, which requires a registered ASIO
event, which the `event.c` abort refuses first. So the abort and the assert
guard the same condition -- the soak exercises scheduler suspension in general
but never the zero-divisor case. Don't read a passing soak as proof the assert
holds; the proof is the reasoning above.

Run: build `use=scheduler_scaling_pthreads,systematic_testing` and run the
generative systematic soak (`test/rt-stress/generative/orchestrate_systematic.py`)
across many seeds -- scaling-on seeds exercise scheduler suspension. The weekly
`determinism_smoke.py` runs `--ponynoscale` only, so it does NOT exercise this
path.
