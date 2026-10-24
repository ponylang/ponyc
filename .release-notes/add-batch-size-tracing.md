## Add per-actor batch size change tracing event

A new `actor_batch_size_changed` tracing event under the `actor_state_change` category fires whenever an actor's batch size changes. The event records the actor's identity, type, and new batch size. It appears alongside the existing `actor_muted`, `actor_overloaded`, and `actor_underpressure` events, making the backpressure mechanism's batch size adjustments visible in the trace.

```json
{"name":"actor_batch_size_changed","cat":"ACTOR: Main","ph":"n","args":{"id":"0x...","type_id":3,"type_name":"Main","batch_size":75}, ...}
```

Batch size decreases by 25 each time an actor sends to an overloaded, muted, or under-pressure receiver, reaching 0 when the actor is muted. It increases by 25 each time the actor hits its batch limit while overloaded, with no ceiling. On recovery — a run where the actor didn't send to overloaded actors — it increases by 25, capped at the default of 100.
