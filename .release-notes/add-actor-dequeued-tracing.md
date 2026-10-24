## Add actor_dequeued tracing event

A new `actor_dequeued` tracing event under the `actor` category fires when an actor is popped from a queue. Combined with the existing `actor_scheduled` and `actor_scheduled_inject` events (enqueue) and `actor_run_start`/`actor_run_stop` (execution), this gives queue wait time: the interval from enqueue to dequeue. Enable it with `--ponytracingcategories "actor"`.

```json
{"name":"actor_dequeued","cat":"ACTOR: Main","ph":"i","args":{"id":"0x...","type_id":3,"type_name":"Main","scheduler_index":1,"from_inject":false,"blocked":false, ...}, ...}
```

The `from_inject` field distinguishes inject-queue pops (`true`, the actor crossed schedulers) from local-queue pops (`false`, consumed by the owning scheduler).
