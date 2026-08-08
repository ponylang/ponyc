# Suspend-and-drain stress engine

Drives the arena allocator's suspend-and-drain path with real
schedulers. Memory is allocated on every scheduler thread, the threads
scale down and suspend, and the memory is then freed from the one
remaining active scheduler — foreign frees whose owners are suspended.
Nothing wakes a suspended thread: a freed block is delivered to its
owner's allocator inbox, and the owner drains that inbox on its
polling tick. The suspended owners must take the memory back through
those tick-drains without any of them coming active for work.

## Phases and what each asserts

1. **Allocate** — `--allocators` actors spread across all schedulers,
   each allocating `--blocks` raw pool blocks of `--block-size` bytes
   (one freed block at the default 512 KiB crosses the allocator's
   dirty-sweep threshold by itself, so its pages return
   deterministically) and handing the pointers to one collector. Raw
   blocks, not Pony objects: freeing them later involves no garbage
   collection and no actor work, so the only machinery between the
   free and the reclaim is the allocator's delivery and drain path.
2. **Scale down** — the allocators go idle; the collector polls
   `pony_active_schedulers` until it reaches `--min-schedulers`.
3. **Drain** — the collector frees every block from its own thread and
   busy-polls `VmRSS` until `--reclaim-fraction` of the payload has
   returned and the active count has held at the floor for a second.
   The busy polling keeps one actor always runnable, so a scheduler
   stays active and the runtime never reaches the global teardown
   that would reclaim the memory and mask the path under test; with
   no other work, the only thing that can bring the memory back is
   each owner draining its own inbox on its polling tick. The collector
   also returns its own held memory each poll through the allocator's
   idle-return path: the allocator holds a bounded cache of freed
   memory regardless of owner until an idle moment, the busy poll loop
   never has one on its own, and a held foreign block keeps its owner's
   slab — and the last of the payload's pages — resident. Brief count
   rises are tolerated (the cycle detector's periodic prod makes an actor
   runnable, and a scheduler activated by real work re-suspends once
   the work is gone); a rise that persists for two seconds fails the
   phase: draining must never raise the count.
4. **Scale up** — fresh parallel work is spawned; the count must rise,
   proving suspended threads still activate after drain episodes. The
   exact interleaving — a scale-up landing while a drain is in flight —
   cannot be timed from Pony code.

Exit 0 with a phase-by-phase report, or 1 naming the failed phase.

## Running

Build the tree's own compiler, then the program with it:

    cmake --preset debug
    cmake --build --preset debug
    build/debug/ponyc --debug test/rt-stress/suspend-drain -o /tmp

The runtime's assertions follow the ponyc build's own configuration: a
debug-configured ponyc bundles a runtime with them on. `--debug` leaves
the program itself unoptimized.

Run with a scheduler floor for phase 2 to reach, and Linux for the
VmRSS read:

    /tmp/suspend-drain --ponyminthreads 1

The floor defaults to 1, and 1 is reachable: `--ponyminthreads 1`
keeps one scheduler always active, and that scheduler runs the poll
timer itself, so the count settles at 1. The machine must provide
more schedulers than the floor — phase 4 needs the count to rise
above it — and more schedulers means more suspended owners. The
polls are bounded, so a runtime bug that leaves the collector
running shows up as a phase failure; one that stops the collector
itself stops the poll clock with it.
