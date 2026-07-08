# Generative backpressure workload depends on the runtime's muting semantics

The `backpressure` workload in `test/rt-stress/generative/main.pony` (the
`Producer`/`Consumer` star) is deadlock-free and terminating only because of three
properties of the runtime's actor muting, in `src/libponyrt/actor/actor.c` and
`src/libponyrt/sched/scheduler.c`. A change to any of them breaks the workload as a
**soak hang** (the orchestrator's liveness timeout), not as a local failure near the
runtime change — and the runtime's own tests do not exercise this combination.

1. **Self-send immunity.** `maybe_mark_should_mute` (actor.c) only mutes a sender
   when `ctx->current != to` — a self-send never mutes the sender. The Consumer's
   `lift` is a self-send, so it is always delivered even while the Consumer is
   UNDER_PRESSURE. If self-sends could mute, the `lift` could be starved and the
   Consumer would never release, hanging the flood.

2. **Explicit release is the sole guaranteed unmute for a pressure-mute.** A producer
   muted for sending to the UNDER_PRESSURE Consumer is unmuted only when the Consumer
   calls `pony_release_backpressure` (via `Backpressure.release` in `lift`/`_finish`).
   The automatic overload-clear path (`actor_unsetoverloaded`) broadcasts its unmute
   only `if (!UNDER_PRESSURE)`, so it is subordinate: draining the Consumer's mailbox
   does NOT unmute its pressure-muted producers while pressure is held. The workload
   relies single-threaded on the explicit release. This is also why the Consumer must
   never do a foreign send (it could be muted itself, with no path to release) — see
   the DEADLOCK-FREEDOM INVARIANT comment at the `Backpressure.apply` site.

3. **A held mute defers quiescence.** A scheduler holding a muted actor does not let
   the program quiesce, so a never-released mute manifests as a hang rather than a
   wrong result.

This is distinct from the systematic-testing thread-handoff mechanism (the re-park
loops in `src/libponyrt/sched/systematic_testing.c`), which is about shutdown
handoff, not normal-mode muting.

Run: the generative `backpressure` soak in both modes — `orchestrate_normal.py`
(`.github/workflows/stress-test-generative-normal-*.yml`) and `orchestrate_systematic.py`
(`.github/workflows/stress-test-generative-systematic-*.yml`); locally `python3
test/rt-stress/generative/orchestrate_normal.py --ponyc <debug ponyc> --count <N>` (or
the systematic orchestrator with a systematic ponyc). A muting-semantics regression
surfaces as a backpressure run timing out (the deadlock-freedom properties above). A
change to the *order* the muteset is drained under systematic instead shows as an
ORDER_SIG divergence — a separate coupling, in `systematic-testing-send-ordering.md`.
