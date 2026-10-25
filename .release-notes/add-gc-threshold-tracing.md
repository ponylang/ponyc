## Add per-actor GC threshold change tracing event

A new `actor_gc_threshold_changed` tracing event under the `actor_gc` category fires at the end of each GC cycle. The event records the actor's identity, type, the new GC threshold (`next_gc`), the heap bytes in use after the sweep (`used`), and whether the hold-on-delete special case fired (`held`).

```json
{"name":"actor_gc_threshold_changed","cat":"ACTOR: Main","ph":"n","args":{"id":"0x...","type_id":3,"type_name":"Main","next_gc":32768,"used":16384,"held":false}, ...}
```

The threshold is normally `used * 2.0`, clamped to a minimum of 16384 bytes. When an actor deletes more than 100 actor references in a single GC cycle, the threshold is held at its current value instead of growing — the `held` field is `true` in that case. This makes GC pacing visible: whether an actor's threshold is growing with its heap, staying flat because of frequent actor reference deletion, or resetting to the minimum after a large sweep.
