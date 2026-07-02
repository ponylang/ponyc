# Generative actorref workload's coverage depends on its cargo being a fresh, unheld actor tag

The `actorref` workload in `test/rt-stress/generative/main.pony` (the `Referrer`,
`Relay`, and `Referent` types) exists to exercise an ORCA path no other workload
reaches under load: reference-counting an **actor** reference (`tag`) passed as
message cargo — the send / recv / **acquire** / release of a foreign actor
reference in `src/libponyrt/gc/gc.c` (`ponyint_gc_sendactor` /
`recv_remote_actor` / `send_remote_actor`'s `acquire_actor` branch) and
`src/libponyrt/gc/actormap.c`'s `send_release`. Measurement (instrumenting the
top-level GC entry points, then reverting) established the gap: the other four
workloads hit `acquire_actor` **0–1 times a whole run** — all at setup wiring —
because they only ever pass actor references inside `val` neighbor/group arrays
held permanently in a field, so the weighted reference count sits at `GC_INC_MORE`
and no send ever needs to re-acquire. `actorref` drives `acquire_actor`
proportional to `chains * ttl`.

That coverage rests on **three** source properties, and a change to any one
**silently loses the coverage while every oracle stays green** — conservation
still counts the same `cite` messages on the live relays, the run does not time
out, and nothing crashes. So neither the workload's own oracles nor the soak can
detect the regression.

1. **The cargo must be an actor `tag`.** `Relay.cite` takes
   `referent: Referent tag` and forwards it. If the cargo ever becomes a `val`
   payload (or any non-actor), it drives the object path
   (`ponyint_gc_sendobject`) the mesh already covers, and the actor-reference
   machinery goes untouched.

2. **A relay must NOT store the referent (nor hold the referent set).** `cite`
   re-sends the just-received tag and drops it — it keeps no `_referent` field and
   no pool. This is load-bearing: a freshly received foreign actor reference lands
   at reference count 1, so the forward hits `send_remote_actor`'s `rc <= 1` branch
   → `acquire_actor`, and dropping it lets the next hop re-acquire. If a relay
   stored the referent (or held the whole pool), that reference's weighted budget
   would sit at `GC_INC_MORE` and every forward would just decrement — `acquire_actor`
   goes cold, exactly the fixed-pool corner measurement rejected. (Holding the
   neighbor RELAYS is fine — a neighbor is a `cite` recipient, not traced cargo.)

3. **Referents must be built FRESH per chain and stay collectable — not a fixed
   pool, not retained.** `Referrer` builds a new `Referent` in the injection loop
   for each chain and never keeps it in a field; the loop's continued allocation
   makes the `Referrer` sweep and release earlier referents, so each becomes garbage
   as its chain drains. A fixed pool (a shared referent forwarded by many relays)
   amortizes the acquire away — measured: `referents=1` was the COLDEST corner
   (acquire ≈ setup only), the opposite of what a naive model predicts. Retaining
   the referents in a field also removes the leak signal: a lost RELEASE only shows
   as memory growth because uncollected referents accumulate in proportion to
   `chains`; a retained (never-freed-anyway) pool would mask it.

This was verified by instrumenting `gc.c`'s top-level GC entry points (counting
foreign actor sends/recvs, object sends/recvs, and `acquire_actor`) and comparing
the fresh-per-chain design against a fixed pool: fresh-per-chain drove
`acquire_actor ≈ chains * ttl` and stayed that way under lazy GC
(`--ponygcinitial 20`), while a 64-referent pool dropped ~6x under lazy GC and a
1-referent pool was flat at ~setup levels. Object sends were 0 (pure actor cargo).

Run: re-run that instrumentation, NOT the soak. Add foreign-actor-send /
foreign-actor-recv / `acquire_actor` counters to `ponyint_gc_sendactor`,
`recv_remote_actor`, and `acquire_actor` in `src/libponyrt/gc/gc.c`, build a debug
ponyc, compile and run the engine `--workload actorref` at a small and a large drawn
shape (and at `--pingers 1`, the self-referral corner), and confirm `acquire_actor`
is proportional to `chains * ttl` (vs 0–1 in every other workload) and object sends
are ~0. Revert the instrumentation afterward. The soak
(`orchestrate_normal.py`/`orchestrate_systematic.py` drawing `actorref`) canNOT
catch any of the three regressions, so it is the wrong check here.
