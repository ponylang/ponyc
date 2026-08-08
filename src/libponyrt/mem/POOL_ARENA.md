# Arena Allocator

The arena allocator reserves large virtual address ranges and carves them into fixed-size arenas, each owned by a single thread. Threads allocate from their own arenas without locks. Ownership lets freed memory be merged through bitmap operations, batched efficiently across threads, and returned to the operating system.

Allocations fall into three size tiers. Small allocations (up to 1 MiB) are carved from slabs within an arena. Large allocations that still fit in an arena occupy a contiguous span of units. Both live inside an arena and inherit its ownership. Allocations too large for an arena get their own standalone mapping with no owner — any thread can free them directly, bypassing the cross-thread delivery mechanism.

## Physical layout

Memory comes from the operating system in three kinds of mapping, identified by a tag in each mapping's header: `MAPPING_REGION`, `MAPPING_ARENA`, and `MAPPING_OVERSIZED`.

### Regions

A region is a single mapping of `REGION_SIZE` bytes — 256 MiB (64 MiB on ILP32) — aligned to its own size. Regions are linked into a global list and never unmapped. Physical pages are committed and decommitted; the virtual address range is permanent. A thread scanning the region list for a free arena slot never races an unmap, because the list only grows and is never modified after publication.

A region is divided into `REGION_ARENAS` slots of `ARENA_SIZE` each. Slot 0 holds the `region_t` header and is never claimed. A 32-bit atomic word records slot occupancy, one bit per slot.

### Arenas

An arena is `ARENA_SIZE` bytes — 8 MiB (2 MiB on ILP32) — carved from a region by setting one bit in the region's slot bitmap with a single compare-and-swap. Each arena has an owner: the thread that claimed it.

An arena is divided into units of `UNIT_SIZE` bytes — 16 KiB (`1 << UNIT_BITS`, where `UNIT_BITS` is 14). `ARENA_UNITS` is `ARENA_SIZE / UNIT_SIZE`: 512 on 64-bit, 128 on ILP32. A bitmap of `BITMAP_WORDS` 64-bit words records which units are in use. A per-unit record array (`arena_unit_t units[ARENA_UNITS]`) holds each unit's state (`UNIT_STATE_FREE`, `UNIT_STATE_HEAD`, or `UNIT_STATE_CONT`), slab bookkeeping, and free-list pointers.

The `arena_t` header occupies the first units of the arena (ceiling of `sizeof(arena_t)` divided by `UNIT_SIZE`). Those header units are marked used when the arena is claimed and never allocated to callers.

Masking a pointer's low bits with `~ARENA_MASK` yields its arena header. The `arena_of` function uses this to locate the arena from any interior pointer.

### Units

A unit is the allocation grain: 16 KiB. A slab is a contiguous run of units all assigned to one size class. No allocator metadata precedes an object. A freed object stores a free-list link in its first word; no other freed-object memory holds allocator state at rest (during cross-thread delivery, a `run_header_t` temporarily occupies the first 24 bytes of the tail object in each run — see "Runs").

## Size tiers

`ponyint_pool_alloc_size` dispatches to one of three tiers based on the requested size. Every allocation path aborts the process on a mapping failure rather than returning NULL. `ponyint_pool_free_size` dispatches the same way: small objects go to `ponyint_pool_free`, oversized mappings go to `oversized_free` on whichever thread frees them, and block-span objects route by ownership — own-thread to `pool_block_free`, foreign-thread to `chain_push`.

### Small (slab)

Sizes up to `POOL_MAX` (1 MiB, `1 << POOL_MAX_BITS`) fall into 16 size classes (`POOL_COUNT`), doubling from `POOL_MIN` (32 bytes, `1 << POOL_MIN_BITS`). These go through `ponyint_pool_alloc`. Each size class has slabs of a fixed unit span: classes up to `UNIT_SIZE` occupy one unit, larger classes occupy `class_size / UNIT_SIZE` units.

### Large (block span)

Sizes above `POOL_MAX` that fit within an arena's usable capacity (total arena bytes minus the header's space) go to `pool_block_alloc`. The allocation is a contiguous run of units inside an arena, tagged with `SLAB_CLASS_BLOCK`. Each block span holds a single object.

### Oversized

Sizes above the arena's usable capacity go to `oversized_alloc`. Each oversized allocation is a standalone mapping, aligned to `ARENA_SIZE`. The payload starts one `UNIT_SIZE` in from the mapping base, so masking the payload pointer finds the `oversized_t` header the same way it finds an arena. The mapping's reserved size is rounded up to the next power of two, with `ARENA_SIZE` as the floor — this guarantees the same pointer-masking that `arena_of` uses for arenas.

An oversized mapping has no owner. Whichever thread frees it runs the stash-or-unmap logic directly, bypassing chain, run, and inbox delivery (see "Cross-thread frees"). This is the one exception to the ownership model for arena-based allocations.

## Allocation and free

### Slab allocation

`ponyint_pool_alloc` searches for space in this order:

1. **Thread cache** — a per-class LIFO stack. If a cached object exists for the class, it is returned immediately.
2. **Current slab** — the `class_slab[index]` for this class. Allocates by bump pointer or from the slab's free list.
3. **Inbox drain** — `inbox_drain` reclaims objects other threads freed to this one (see "Cross-thread frees"). The drain may refill the current slab; if so, the current slab is retried.
4. **Partial slab** — a slab on the `partial_slabs[index]` list, which holds slabs with available space. The partial slab becomes the new current slab.
5. **Slab reserve** — `slab_reserve` scans the thread's owned arenas for a free run of the required span. Single-unit slabs are carved from the low end of the arena; multi-unit spans from the high end. This separation preserves long free runs for future block-span allocations.
6. **New arena** — `arena_new` claims a fresh arena from a region (or maps a new region). `slab_reserve` is retried on the new arena.

### Slab free

`ponyint_pool_free` routes freed small objects:

1. If the thread cache for this class is not full, the object goes into the cache regardless of which thread owns it. Ownership is deferred until the object leaves the cache.
2. If the cache is full and the object belongs to this thread (`arena->owner_slot == this_thread.slot`), the object goes to `slab_free`, which adds it to its slab's free list. When a slab's live count reaches zero, `slab_release` returns its units to the arena — unless it is the current slab for its class, which is reset in place (bump pointer to zero, free list cleared) to avoid a release-and-re-reserve cycle.
3. If the cache is full and the object belongs to another thread, the object goes to `chain_push`, which places it on that owner's chain for eventual delivery through the inbox mechanism (see "Cross-thread frees").

### Block-span allocation and free

`pool_block_alloc` drains the inbox, then calls `slab_reserve` for a contiguous unit run of the required span. If no arena has room, `arena_new` claims one and retries.

`pool_block_free` (own-thread path) decrements the slab's live count and calls `slab_release`. The foreign-thread path goes through `chain_push`, the inbox, `apply_run`, and then `slab_release`.

### Oversized allocation and free

`oversized_alloc` first searches the thread's `oversized_stash` for a mapping with the same reservation key (power-of-two reserved size). A match needs no new mapping call; if the new payload is larger than what was previously committed, committed bytes are extended. On a miss, a new mapping is reserved and committed.

`oversized_free` stashes the mapping if the committed bytes fit under the large-retention byte budget (see "Large retention"). When the mapping does not fit, the stash is scanned for the oldest differently-keyed mapping; if evicting that mapping frees enough space, the two are swapped (same-key entries are never evicted). If no eviction frees enough space, the mapping is unmapped.

### Realloc

`ponyint_pool_realloc_size` returns the old pointer when the old and new sizes fall in the same size class (for small allocations) or have the same adjusted size (the requested size rounded up to `POOL_ALIGN`, for large and oversized). Otherwise it allocates at the new size, copies `min(old_size, new_size)` bytes, and frees the old block.

### Alignment

Every allocation is aligned to at least `POOL_MIN` (32 bytes). Once the granted size reaches `POOL_ALIGN` (1 KiB), alignment is at least `POOL_ALIGN`. The `POOL_MIN` guarantee is declared to LLVM as the `align` return attribute on the pool allocators; the `POOL_ALIGN` tier is a property of the allocator's geometry, not representable as a static LLVM attribute.

## Cross-thread frees

Foreign-freed objects reach their owner through the chain, run, and inbox stages below. No step reads or writes the owning thread's per-slab bookkeeping until the owner itself drains.

### Chains

Each thread has an open-addressing hash map of `chain_t` entries, keyed by owner slot. `chain_push` appends a freed object to the head of the chain for that owner and increments the chain's count.

### Runs

A "run" in this section is a batch of freed objects grouped for cross-thread delivery (the `run_header_t` structure), unrelated to the "contiguous run of units" that describes a slab's span elsewhere in this document.

When a chain's count reaches `BATCH_SIZE` (32), or at a flush, the freeing thread calls `chain_flush`. `chain_flush` sorts the chain's objects into per-unit groups. Within each group, the objects are linked through their first words. The last object's first bytes are overwritten with a `run_header_t` (24 bytes on 64-bit) containing the run's first-object pointer, length, and a link to the next run. All runs are linked into a list.

### Inbox delivery

The freeing thread pushes the run list onto the owner's inbox (`inbox_t.head`) with a single atomic CAS. An inbox is a single atomic pointer per owner slot. Slots reside in a global registry of `inbox_segment_t` segments, each holding `SEGMENT_SLOTS` (256) inboxes. Segments are appended with CAS and never freed.

### Inbox drain

`inbox_drain` takes the entire inbox with one atomic exchange and walks the run list, calling `apply_run` on each run. `apply_run` splices the run's objects onto the slab's free list and decrements the slab's live count. For block-span runs (single object, `SLAB_CLASS_BLOCK`), it calls `slab_release` directly.

Drain runs on the allocation slow path (both `ponyint_pool_alloc` and `pool_block_alloc`), on `ponyint_pool_drain`, on `ponyint_pool_suspend_flush` (which also flushes this thread's outgoing chains before draining — called when a scheduler thread parks before sleeping), and at thread exit (`ponyint_pool_thread_cleanup`).

## Memory return

Three mechanisms return physical pages to the operating system.

### Decommit immediate

When `slab_release` frees a small-class slab whose span is at or above `active_decommit_immediate_span` (default `ARENA_UNITS / 4` = 128 units on 64-bit, 32 on ILP32), `slab_release` decommits the pages at once. Block-span frees bypass this threshold check: they go through large retention (see "Large retention") and are either retained or decommitted immediately depending on the budget.

### Dirty sweep

Freed slabs smaller than the immediate-decommit span leave their pages committed as dirty units. The `dirty` bitmap in the arena records them. When the count of dirty units not marked in `dirty_large` (`dirty_units - large_dirty_units`) reaches `active_dirty_sweep_threshold` (default `ARENA_UNITS / 16` = 32 units on 64-bit, 8 on ILP32), `dirty_sweep_small` coalesces and decommits those dirty units. Units marked in the `dirty_large` bitmap (see "Large retention") are not touched by the small sweep.

### Idle return

When a parked scheduler thread's idle-backoff timer (`SCHED_TICK_MAX_NS`) reaches its 500 ms cap, `ponyint_pool_return_idle` runs:

1. Flush the thread cache, routing each object by ownership.
2. Deliver all pending chains.
3. Call `large_retain_purge` — decommits every dirty unit in every owned arena (including those marked in `dirty_large`) and unmaps every stashed oversized mapping.
4. Release surplus empty arenas beyond one reserve.

After idle return, the thread holds no committed freed allocation memory. The chain map — an allocator-internal hash table sized to the peak number of distinct owners this thread has freed to — stays allocated until thread exit.

On a saturated thread — one that never idles — idle return never runs and retained memory stays committed.

## Thread cache

The thread cache is used only for the small (slab) tier. Each size class has a LIFO stack of up to `POOL_CACHE_DEPTH` (512) cached objects, stored in `pool_cache[POOL_COUNT][POOL_CACHE_DEPTH]`. The cache holds objects regardless of which thread owns them.

The per-class depth is computed by `cache_cap`: `max(active_cache_budget / class_size, active_cache_floor)`, capped at `POOL_CACHE_DEPTH`. Without the floor, large classes fall to a depth of one or two, causing a slab reserve and release on every churn cycle. The budget and floor are set by the memory profile (see "The tuning dial").

The cache is flushed on idle return and at thread exit. `cache_flush_routed` routes each cached object by ownership: own-thread objects go to `slab_free`, foreign objects go to `chain_push`.

In debug and release-safe (compiled with `-DPONY_ALWAYS_ASSERT`) builds, `POOL_CACHE_SENTINEL` (`0xCAC4E5A17ECAC4E5`) is written into a cached object's first word on push and checked on pop. A double free that re-pushes a cached object fires the check. See "Debug facilities" for the full list of checks.

## Large retention

A single per-thread ledger (`large_retain_held`) is checked against a byte budget (`active_large_retain`) to limit how much freed block-span and oversized memory a thread may leave committed for reuse.

`fits_large_retain` checks whether adding a candidate's bytes would stay within the budget. Admission is all-or-nothing: a block span that does not fit is decommitted immediately; an oversized mapping that does not fit is unmapped. Nothing already retained is evicted for a new candidate — eviction costs two OS interactions (decommit the evicted mapping, refault it later) where rejection costs one. The one exception: when the stash holds a mapping under a different reservation key and evicting that mapping would let the new one fit, the old mapping is unmapped and the new one stashed.

Freed block spans admitted under the budget are marked in the arena's `dirty_large` bitmap by `retain_span`. The small dirty sweep skips these units. Freed oversized mappings admitted under the budget go onto `this_thread.oversized_stash`.

`large_retain_purge` decommits every dirty unit in every owned arena (`dirty_large` and ordinary dirty alike), unmaps all stashed oversized mappings, and zeroes the held counter. It runs on idle return and at thread exit.

## Owner registry

Owner slots are assigned from a global monotonic counter (`next_owner_slot`), incremented by atomic fetch-add. Slots are never reused, because reuse would require proof that no other thread holds the old slot in a chain or is about to push to its inbox.

Inboxes reside in `inbox_segment_t` segments (see "Inbox delivery"), each at cache-line alignment (one `inbox_t` per cache line). A resolved inbox pointer stays valid for the life of the process.

A slot is assigned when the thread reserves its first arena, not on the first allocation. A thread that only frees has no slot, and none is required: no other thread can address freed objects to it.

An exited thread's inbox persists as one cache line in a segment.

## Thread cleanup

`ponyint_pool_thread_cleanup` runs at thread exit. It flushes the cache (routing by ownership), delivers all pending chains, drains the inbox, purges all retained pages and stashed mappings, and frees the chain map. Arenas stay mapped — other threads may still hold live objects in them.

## Empty-arena policy

`slab_release` retains one empty arena as a reserve rather than releasing its slot, so churn across the arena boundary does not cost a release and re-claim each cycle. When two empty arenas coexist, the clean one (no retained units) is released.

Under large retention, freed block spans in an empty arena may be retained. Up to `LARGE_RETAIN_EMPTY_CAP - 1` (3) retained empty arenas may persist. When the count reaches `LARGE_RETAIN_EMPTY_CAP` (4), the retained pages in the newly emptied arena are decommitted and the slot is released.

Idle return releases empty arenas down to one.

## The tuning dial

`--ponymemoryprofile` sets a memory profile at startup, from rung 1 (smallest footprint) to rung 10 (most throughput). The default is rung 3. `ponyint_pool_set_memory_profile` writes the profile's values into the active thresholds once, before any scheduler thread starts; the free path reads them without synchronization.

Each profile sets five parameters:

- **floor** (`active_cache_floor`) — minimum per-class cache depth, regardless of the byte budget.
- **budget** (`active_cache_budget`) — bytes of cache per size class. A class's cache depth is `max(budget / class_size, floor)`, capped at `POOL_CACHE_DEPTH` (512).
- **span** (`active_decommit_immediate_span`) — decommit-immediate threshold, in units. A freed slab at or above this span has its pages returned at once.
- **threshold** (`active_dirty_sweep_threshold`) — dirty-sweep trigger, in units. When non-retained dirty units in an arena reach this count, a sweep coalesces and decommits them.
- **large_retain** (`active_large_retain`) — large-retention byte budget. Bytes of freed block-span and oversized memory a thread may leave committed.

The `span` and `threshold` columns in `memory_profiles[]` are expressed against a reference arena of `PROFILE_TUNED_UNITS` (512) units and scaled to `ARENA_UNITS` at runtime: `actual = (table_value * ARENA_UNITS) / PROFILE_TUNED_UNITS`. On 64-bit (`ARENA_UNITS` = 512) the values are used as-is. On ILP32 (`ARENA_UNITS` = 128) they scale down by 4x.

| Rung | floor | budget   | span (ref 512) | threshold (ref 512) | large_retain |
|------|-------|----------|----------------|---------------------|--------------|
| 1    | 0     | 64 KiB   | 1              | 4                   | 0            |
| 2    | 4     | 256 KiB  | 64             | 16                  | 2 MiB        |
| 3*   | 8     | 640 KiB  | 128            | 32                  | 16 MiB       |
| 4    | 16    | 768 KiB  | 256            | 64                  | 16 MiB       |
| 5    | 24    | 1 MiB    | 512            | 128                 | 24 MiB       |
| 6    | 32    | 1 MiB    | 512            | 160                 | 32 MiB       |
| 7    | 48    | 1.5 MiB  | 512            | 256                 | 48 MiB       |
| 8    | 64    | 1.5 MiB  | 512            | 384                 | 64 MiB       |
| 9    | 96    | 2 MiB    | 512            | 512                 | 96 MiB       |
| 10   | 128   | 2 MiB    | 512            | 512                 | 128 MiB      |

\* Default rung. Values are from the `memory_profiles[]` array.

## Debug facilities

Debug builds (and release-safe builds with `-DPONY_ALWAYS_ASSERT`) enable several checks.

### POISON (slab)

Debug-only (`#ifndef PONY_NDEBUG`). The constant `0xDEADFA11DEADFA11` is written into the first word of each unit of a released slab. When a unit whose pages were never decommitted is re-reserved, an assert checks that the word still holds. A program that wrote to a freed unit's first word in that window fails the assert.

### POISON (stash)

Debug and release-safe (gated like `ARENA_CHECK`). The oversized stash reuses `POISON` as its guard value (`stash_validate_push` / `stash_validate_pop`).

### ARENA_CHECK

Asserts throughout the allocator that verify mapping-kind tags, ownership, bitmap consistency, run validity, and slab invariants. Active in debug builds and in release builds compiled with `-DPONY_ALWAYS_ASSERT`.

### Mapping-kind tags

Every mapping header contains a `kind` field (`MAPPING_REGION`, `MAPPING_ARENA`, `MAPPING_OVERSIZED`). `ARENA_CHECK` verifies the tag on each free path.

### Cache sentinel

See "Thread cache" for the `POOL_CACHE_SENTINEL` double-free check. Gated like `ARENA_CHECK`.

The arena backend is incompatible with AddressSanitizer, Valgrind, and pooltrack.
