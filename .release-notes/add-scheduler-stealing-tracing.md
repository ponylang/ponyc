## Add scheduler work-stealing tracing events

Two new tracing events under the `scheduler_stealing` category record work-stealing activity: `steal_successful` fires when a scheduler takes an actor from another scheduler's queue, and `steal_failure_round` fires when a scheduler completes a full steal round without finding work. Enable them with `--ponytracingcategories "scheduler_stealing"`.

```json
{"name":"steal_successful","cat":"scheduler_stealing","ph":"i","args":{"thief_index":2,"victim_index":0,"actor":"0x...","type_id":3,"type_name":"Main"}, ...}
{"name":"steal_failure_round","cat":"scheduler_stealing","ph":"i","args":{"thief_index":2}, ...}
```

`steal_successful` identifies the thief, the victim, and the stolen actor. `steal_failure_round` identifies only the thief — no steal occurred, so there is no victim.
