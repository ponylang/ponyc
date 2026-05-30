## Fix compiler crash when a generic with a constructable union constraint is instantiated with a union

Previously, instantiating a generic whose type parameter had a *constructable union constraint* — a union whose members can all be constructed, such as `(Default val | None val)` — with a non-concrete type argument like another union crashed the compiler with an assertion failure:

```pony
primitive A
primitive B

interface val Default
  new val create()

class Generic[T: (Default val | None val)]
  let x: T = T.create()
  fun get(): T => x

actor Main
  new create(env: Env) =>
    // Crashed the compiler.
    let bad = Generic[(A | B)].get()
```

The compiler now rejects this at compile time with a clear error instead of crashing:

```
Error:
a constructable constraint can only be fulfilled by a concrete type argument
```
