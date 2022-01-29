## Remove simplebuiltin compiler option

The ponyc option `simplebuiltin` was never useful to users but had been exposed for testing purposes for Pony developers. We've removed the need for the "simplebuiltin" package for testing and have remove both it and the compiler option.

Technically, this is a breaking change, but no end-users should be impacted.

## Fix return checking in behaviours and constructors

Our checks to make sure that the usage of return in behaviours and constructors was overly restrictive. It was checking for any usage of `return` that had a value including when the return wasn't from the method itself.

For example:

```pony
actor Main
  new create(env: Env) =>
    let f = {() => if true then return None end}
```

Would return an error despite the return being in a lambda and therefore not returning a value from the constructor.

## Fix issue that could lead to a muted actor being run

A small logical flaw was discovered in the Pony runtime backpressure system that could lead to an actor that has been muted to prevent it from overloading other actors to be run despite a rule that says muted actors shouldn't be run.

The series of events that need to happen are exceedingly unlikely but we do seem them from time to time in our Arm64 runtime stress tests. In the event that a muted actor was run, if an even more unlikely confluence of events was to occur, then "very bad things" could happen in the Pony runtime where "very bad things" means "we the Pony developers are unable to reason about what might happen".

