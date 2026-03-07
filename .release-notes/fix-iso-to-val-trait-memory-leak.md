## Fix memory leak when a behavior parameter has a different capability than the trait it implements

When a behavior was called through a trait reference and the concrete actor's parameter had a different trace-significant capability (e.g., the trait declared `iso` but the actor declared `val`), the ORCA garbage collector's reference counting was broken. The sender traced the parameter with one trace kind and the receiver traced with another, causing field reference counts to never reach zero. Objects reachable from the parameter were leaked.

```pony
trait tag Receiver
  be receive(b: SomeClass iso)

actor MyActor is Receiver
  be receive(b: SomeClass val) =>
    // b's fields were leaked when called through a Receiver reference
    None
```

The sender and receiver now use consistent tracing for each parameter, regardless of capability differences between the trait and concrete method.
