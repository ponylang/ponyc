## Fix the capability of a partially applied constructor

Partially applying a zero-argument constructor produced an object that could not be called. The generated `apply` had a `ref` receiver, but the object itself was `val`, so calling it failed with "receiver type is not a subtype of target type."

```pony
class Bar
  new create() => None

actor Main
  new create(env: Env) =>
    let mk = Bar~create()
    let b = mk()  // failed: {(): Bar ref^} val is not a subtype of {(): Bar ref^} ref^
```

The same problem prevented annotating the partial as `{(): Bar ref^} val` or passing it to a behavior, even when all captured arguments were sendable.

The generated `apply` now gets a `box` receiver when no captured argument prevents it, so `val` objects can be called and sent across actors.
