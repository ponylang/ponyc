"""
# Runtime Tracing Package

The Runtime Tracing package exposes the runtime's event tracing infrastructure
to Pony programs. Tracing records scheduler, actor, and GC events as Chrome
Trace Event JSON, either to a file or to an in-memory flight recorder that
dumps on crash.

Categories control which kinds of events the runtime records. They are
configured at startup via `--ponytracingcategories` and can be changed at
runtime through this package. The three category operations are:

* `enable_categories` — add the listed categories to the active set.
* `disable_categories` — remove the listed categories from the active set.
* `set_categories` — replace the active set with exactly the listed categories.

Each category is a primitive passed in an array:

```pony
RuntimeTracing.enable_categories(
  auth, [SchedulerTracing; ActorBehaviorTracing])
```

The available categories are: `SchedulerTracing`, `SchedulerMessagingTracing`,
`ActorTracing`, `ActorBehaviorTracing`, `ActorGCTracing`,
`ActorStateChangeTracing`, `SystematicTestingTracing`,
`SystematicTestingDetailsTracing`, `RuntimeLifecycleTracing`,
`SchedulerStealingTracing`, and `CycleDetectorTracing`.

Actor event categories (`ActorTracing`, `ActorBehaviorTracing`, etc.) trace
all actors. `CycleDetectorTracing` enables actor events for the cycle detector
only, without enabling them for every actor.

## Example

```pony
use "runtime_tracing"

actor Main
  new create(env: Env) =>
    let auth = RuntimeTracingAuth(env.root)

    // Turn on scheduler and actor behavior tracing at runtime.
    RuntimeTracing.enable_categories(
      auth, [SchedulerTracing; ActorBehaviorTracing])

    env.out.print("tracing enabled")

    // Later, disable all categories.
    RuntimeTracing.set_categories(auth, [])
```
"""

use @pony_tracing_enable_categories[Bool](categories: U64)
use @pony_tracing_disable_categories[None](categories: U64)
use @pony_tracing_set_categories[Bool](categories: U64)

primitive RuntimeTracing
  """
  Toggle runtime tracing categories.
  """
  fun enable_categories(
    auth: RuntimeTracingAuth,
    categories: ReadSeq[TracingCategory] box)
    : Bool
  =>
    """
    Add the listed categories to the active set. Opens the trace output file
    on the first call that enables any category. Returns false if the output
    file cannot be opened.
    """
    @pony_tracing_enable_categories(_bitmask(categories))

  fun disable_categories(
    auth: RuntimeTracingAuth,
    categories: ReadSeq[TracingCategory] box)
  =>
    """
    Remove the listed categories from the active set.
    """
    @pony_tracing_disable_categories(_bitmask(categories))

  fun set_categories(
    auth: RuntimeTracingAuth,
    categories: ReadSeq[TracingCategory] box)
    : Bool
  =>
    """
    Replace the active category set with exactly the listed categories. Pass
    an empty collection to disable all categories. Returns false if the output
    file cannot be opened.
    """
    @pony_tracing_set_categories(_bitmask(categories))

  fun _bitmask(categories: ReadSeq[TracingCategory] box): U64 =>
    var mask: U64 = 0
    for cat in categories.values() do
      mask = mask or cat.apply()
    end
    mask
