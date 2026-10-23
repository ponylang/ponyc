## Add actor scheduling tracing events

Two new tracing events fire when an actor is placed onto a scheduler's run queue: `actor_scheduled` (local scheduler queue) and `actor_scheduled_inject` (global inject queue). The timestamp difference between `actor_scheduled` and the next `actor_run_start` for the same actor is the queue-wait time — how long the actor sat in a queue before running. Enable them with `--ponytracingcategories "actor"`.

`actor_scheduled` includes a `scheduler_index` field identifying which scheduler's queue the actor was pushed to. `actor_scheduled_inject` omits it because the inject queue is global — the actor is popped by whichever scheduler checks the inject queue first.

```json
{"name":"actor_scheduled","cat":"ACTOR: MyActor","ph":"n","args":{"type_name":"MyActor","scheduler_index":3, ...}, ...}
{"name":"actor_scheduled_inject","cat":"ACTOR: cycle detector","ph":"n","args":{"type_name":"cycle detector", ...}, ...}
```
