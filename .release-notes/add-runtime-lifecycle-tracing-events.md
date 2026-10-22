## Add runtime lifecycle tracing events

Three new tracing events mark the runtime's lifecycle: `runtime_init` (after scheduler creation), `runtime_start` (when schedulers begin running), and `runtime_shutdown` (after the runtime finishes, with the exit code). Enable them with `--ponytracingcategories "runtime"` or include them in a wildcard (`"*"`).

```pony
// compile and run with tracing enabled:
// ./ponyc myapp
// ./myapp --ponytracingcategories "runtime" --ponytracingoutput trace.json
```

The trace output includes three instant events with `"cat": "runtime"`:

```json
{"name":"runtime_init","cat":"runtime","args":{"scheduler_count":8,"min_scheduler_count":0}, ...}
{"name":"runtime_start","cat":"runtime", ...}
{"name":"runtime_shutdown","cat":"runtime","args":{"exit_code":0}, ...}
```
