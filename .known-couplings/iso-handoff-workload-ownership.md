# Generative iso-handoff workload's coverage depends on its cargo staying iso + multi-object

The `iso` workload in `test/rt-stress/generative/main.pony` (the `Dispatcher`,
`Carrier`, and `Parcel` types) exists to exercise an ORCA trace path no
other workload reaches: transferring ownership of a nested **mutable** object
subgraph between actors, so the receiver acquires a foreign mutable multi-object
graph (the `invent references and acquire` path in `src/libponyrt/gc/gc.c` taken
with `mutability == PONY_TRACE_MUTABLE`). That coverage rests on two source
properties, and a change to either **silently loses the coverage while every oracle
stays green** — conservation still counts the same messages, the parcel-integrity
check still passes, and the soak does not time out. So neither the workload's own
oracles nor the normal-mode soak can detect the regression.

1. **The cargo must be `iso`.** A `Parcel` graph is constructed `recover iso` and
   `Carrier.carry` takes `parcel: Parcel iso` and re-forwards it with `consume`. If
   the constructor or the behavior parameter ever becomes `val` (or anything
   non-`iso`), the graph is traced `PONY_TRACE_IMMUTABLE`, not mutable — it becomes
   the same coverage the existing `String val` payload already gives, and the
   workload no longer earns its keep. The type system enforces the *move* (an iso
   parameter cannot be aliased), but it does NOT stop someone changing the cap.

2. **The graph must stay multi-object — `let`, not `embed`.** `Parcel.kids:
   Array[Parcel]` and the `bytes` arrays are `let` (separate heap allocations) on
   purpose, so the receiver acquires each node as a distinct foreign mutable object.
   pony-ref gotcha #11 ("prefer embed for class-typed fields") points a maintainer
   straight at changing them to `embed` — which keeps the cargo `iso` (so property
   1's check would not fire) but co-allocates the child nodes into their parent,
   collapsing the multi-object subgraph. The node-by-node mutable acquire the
   workload targets shrinks, again with every oracle still green. The comments at
   the field declarations flag this; do not "optimize" them to `embed`. (The graph's
   depth/breadth is a swarm-drawn knob, so the node count varies per run, but a
   single node still has its byte array as a separate allocation — the multi-object
   property must hold even at the smallest drawn shape.)

This was verified by instrumenting `gc.c` (counting `PONY_TRACE_MUTABLE` sends and
the foreign-acquire branches) and comparing the `let`/iso nested graph against a
`val`/`embed` equivalent: the iso multi-object graph drove the mutable-acquire path
heavily (invent_mut high, invent_immut zero) while the alternatives did not.

Run: re-run that instrumentation, NOT the soak. Add the mut/immut/invent_mut
counters to `ponyint_gc_sendobject`/`ponyint_gc_recvobject` and `send_remote_object`
in `src/libponyrt/gc/gc.c`, build a debug ponyc, compile and run the engine
`--workload iso` at a small and a large drawn shape, and confirm typed structs are
traced mutable and the receiver hits the mutable invent-and-acquire branch
(`invent_mut > 0`, `invent_immut ~= 0`). Revert the instrumentation afterward. The
normal-mode soak (`orchestrate_normal.py` drawing `iso`) canNOT catch either
regression, so it is the wrong check here.
