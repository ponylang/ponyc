# `PONY_SCHED_SLEEP_WAKE_SIGNAL` (`src/libponyrt/sched/scheduler.h`) ↔ `Sig.usr2()` (`packages/signals/sig.pony`)

On default builds (not `scheduler_scaling_pthreads`), the runtime reserves SIGUSR2 as its scheduler sleep/wake signal: `PONY_SCHED_SLEEP_WAKE_SIGNAL` is defined to `SIGUSR2`, every runtime thread blocks it, suspended schedulers consume it via `sigwait`, and wakes are delivered with `pthread_kill`. A `SignalHandler` registered for SIGUSR2 on such a build never fires. When built with `scheduler_scaling_pthreads` (forced on macOS), the runtime uses pthread condition variables instead and leaves SIGUSR2 free, so the `signals` package may hand it out.

`Sig.usr2()` mirrors this: it yields a signal number only under `ifdef "scheduler_scaling_pthreads"` and is a `compile_error` otherwise. The two must stay in step — if the runtime's choice of sleep/wake signal changes, or the gate that reserves it moves, `Sig.usr2()`'s gate must change to match, or the package will again advertise a signal the runtime has taken. Both ends carry comments pointing at each other.

One residual gap is not gated: `runtime_tracing` builds reuse SIGUSR2 for the tracing thread (`PONY_TRACING_SLEEP_WAKE_SIGNAL` in `src/libponyrt/tracing/tracing.c`) regardless of `scheduler_scaling_pthreads`, but `runtime_tracing` is not exposed as a Pony `ifdef` userflag, so `sig.pony` cannot exclude it.

Run: the `signals` suite in the stdlib tests (`ctest --preset debug -R stdlib-release` / `ctest --preset debug -R stdlib-debug`) — the `signals/USR2` test exercises SIGUSR2 delivery, but only on `scheduler_scaling_pthreads` builds (e.g. macOS CI).
