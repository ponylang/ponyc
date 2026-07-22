# Suspend-and-drain stress engine

Drives the arena allocator's suspend-and-drain path with real
schedulers. Memory is allocated on every scheduler thread, the threads
scale down and suspend, and the memory is then freed from the one
remaining active scheduler — foreign frees whose owners are asleep.
The suspended owners must take the memory back through their drain
wakes without any of them coming active for work.

## Phases and what each asserts

1. **Allocate** — `--allocators` actors spread across all schedulers,
   each allocating `--blocks` raw pool blocks of `--block-size` bytes
   (the default 512 KiB makes every block an immediate-decommit span,
   so its return is deterministic) and handing the pointers to one
   collector. Raw blocks, not Pony objects: freeing them later
   involves no garbage collection and no actor work, so the only
   machinery between the free and the reclaim is the allocator's
   delivery and drain path.
2. **Scale down** — the allocators go idle; the collector polls
   `pony_active_schedulers` until it reaches `--min-schedulers`.
3. **Drain** — the collector frees every block from its own thread and
   busy-polls `VmRSS` until `--reclaim-fraction` of the payload has
   returned and the active count has held at the floor for a second.
   The busy polling keeps the runtime's quiescence probes — which wake
   every suspended thread — from firing, so the owners' drain wakes
   are the only reclaim path. Brief count rises are tolerated (the
   cycle detector's periodic prod can wake a scheduler that finds no
   work and re-suspends); a rise that persists for two seconds fails
   the phase: a drain wake must never raise the count.
4. **Scale up** — fresh parallel work is spawned; the count must rise,
   proving suspended threads still activate after drain episodes. This
   asserts activation-after-draining; the precise scale-up-lands-mid-
   drain interleaving cannot be timed from Pony code.

Exit 0 with a phase-by-phase report, or 1 naming the failed phase.

## Running

Build:

    ponyc --debug test/rt-stress/suspend-drain -o /tmp

The runtime's assertions follow the ponyc build's own configuration: a
debug-configured ponyc bundles a runtime with them on. `--debug` leaves
the program itself unoptimized.

Run with a scheduler floor for phase 2 to reach, and Linux for the
VmRSS read:

    /tmp/suspend-drain --ponyminthreads 1 --min-schedulers 2

The floor is 2, not 1: polling rides a timer, and every timer fire
wakes a scheduler to run the poll, so the count never settles at 1.
Any scheduler count works; more schedulers means more suspended owners. The polls are bounded, so a runtime bug
that leaves the collector running shows up as a phase failure; one that
stops the collector itself stops the poll clock with it.
