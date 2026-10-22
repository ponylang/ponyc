## Compile runtime bitcode with widest CPU features

The runtime bitcode (`libponyrt.bc`) is now compiled with the widest CPU feature set for the architecture — `x86-64-v4` on x86-64, `armv8.9-a` with all non-SVE extensions on aarch64 — instead of the generic baseline used for the release executables. The user's `--cpu` and `--features` flags still control the final codegen; the wider compilation only affects LLVM's mid-level optimisation decisions in the runtime code, producing better default performance for programs compiled with released ponyc.

## Fix runtime bitcode not benefiting from target-specific optimisations

The runtime bitcode shipped in releases was compiled with the generic CPU baseline (`x86-64`, `armv8-a`), so LLVM's mid-level optimisations — vectorisation width, instruction pattern selection — used the most conservative assumptions. Since ponyc re-targets the runtime bitcode to the user's CPU at compile time, the compilation-time `-march` only affects those mid-level decisions, not the final instruction selection. The runtime is now compiled with the widest feature set for each architecture, so those optimisation decisions are as good as possible regardless of what the user targets.
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

