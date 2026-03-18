## Fix memory leak

When a behavior was called through a trait reference and the concrete actor's parameter had a different trace-significant capability (e.g., the trait declared `iso` but the actor declared `val`), the ORCA garbage collector's reference counting was broken. The sender traced the parameter with one trace kind and the receiver traced with another, causing field reference counts to never reach zero. Objects reachable from the parameter were leaked.

```pony
trait tag Receiver
  be receive(b: SomeClass iso)

actor MyActor is Receiver
  be receive(b: SomeClass val) =>
    // b's fields were leaked when called through a Receiver reference
    None
```

The leak is not currently active because `make_might_reference_actor` — an optimization that was disabled as a safety net — masks it by forcing full tracing of all immutable objects. Without this fix, the leak would have become active as we started re-enabling that optimization.

This applies to simple nominal parameters as well as compound types — tuples, unions, and intersections — where the elements have different capabilities between the trait and concrete method.

The sender and receiver now use consistent tracing for each parameter, regardless of capability differences between the trait and concrete method.
