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

## Fix URI.string() producing ambiguous serializations

`URI.string()` could produce serializations that re-parse as a different URI in two cases, both from RFC 3986 section 3.3.

When a URI had no authority and the path started with `//`, the serialized form was ambiguous — a parser reads the `//` as the authority introducer. `NormalizeURI` and `ResolveURI` could produce such URIs when dot-segment removal turned a path like `/.//evil.com/p` into `//evil.com/p`. `URI.string()` now prefixes `/.` before the path in this case.

```pony
match ParseURI("http:/.//evil.com/p")
| let u: URI val =>
  match NormalizeURI(u)
  | let n: URI val =>
    n.string()
    // was: "http://evil.com/p" (re-parses with host evil.com)
    // now: "http:/.//evil.com/p" (re-parses with no authority)
  end
end
```

When a relative-path reference (no scheme, no authority) had a first path segment containing `:`, the serialized form was ambiguous — a parser reads the text before the colon as a scheme. `NormalizeURI` could produce such URIs when dot-segment removal stripped a leading `./` from a path like `./foo:bar`. `URI.string()` now prefixes `./` before the path in this case.

```pony
match ParseURI("./foo:bar")
| let u: URI val =>
  match NormalizeURI(u)
  | let n: URI val =>
    n.string()
    // was: "foo:bar" (re-parses with scheme "foo")
    // now: "./foo:bar" (re-parses as a relative-path reference)
  end
end
```

## Fix compiler crash on unterminated escape sequence at end of file

When a source file ended with a backslash inside a character or string literal — `let x = '\` or `let x = "\` at the last byte — `ponyc` aborted with an assertion failure instead of reporting a syntax error. The lexer now reports the incomplete escape and the unterminated literal as ordinary compile errors.

## Fix F64.f32() and U128.f32() returning inf for values that round to F32.max_value()

The safe `f32()` conversion on `F64` and `U128` returned `inf` for values slightly above `F32.max_value()` that IEEE 754 round-to-nearest-even rounds to `F32.max_value()`. The unsafe `f32_unsafe()` and `String.f32()` already returned `F32.max_value()` for the same inputs, so `"3.402823567e38".f32()?` returned `F32.max_value()` but `"3.402823567e38".f64()?.f32()` returned `inf`.

```pony
let above = F64.from_bits(F32.max_value().f64().bits() + 1)
above.f32()         // was: inf       now: F32.max_value()
above.f32_unsafe()  // was: F32.max_value() (unchanged)
```

## Add actor scheduling tracing events

Two new tracing events fire when an actor is placed onto a scheduler's run queue: `actor_scheduled` (local scheduler queue) and `actor_scheduled_inject` (global inject queue). The timestamp difference between `actor_scheduled` and the next `actor_run_start` for the same actor is the queue-wait time — how long the actor sat in a queue before running. Enable them with `--ponytracingcategories "actor"`.

`actor_scheduled` includes a `scheduler_index` field identifying which scheduler's queue the actor was pushed to. `actor_scheduled_inject` omits it because the inject queue is global — the actor is popped by whichever scheduler checks the inject queue first.

```json
{"name":"actor_scheduled","cat":"ACTOR: MyActor","ph":"n","args":{"type_name":"MyActor","scheduler_index":3, ...}, ...}
{"name":"actor_scheduled_inject","cat":"ACTOR: cycle detector","ph":"n","args":{"type_name":"cycle detector", ...}, ...}
```

## Fix U128 to F32 conversion returning infinity for values near F32.max_value()

On platforms where the compiler uses the Pony source implementation for 128-bit conversions (Windows MSVC), converting a U128 value slightly above `F32.max_value().u128()` to F32 incorrectly returned infinity instead of `F32.max_value()`. The intermediate F64 representation could be one ULP above `F32.max_value().f64()`, which tripped an overly strict overflow check. The F64-to-F32 conversion already handles rounding correctly on its own.

