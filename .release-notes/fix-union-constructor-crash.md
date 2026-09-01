## Fix compiler crash when instantiating a generic with a union type argument that calls a constructor

Previously, instantiating a generic class that calls `T.create()` with a union type argument crashed the compiler with an assertion failure:

```pony
interface val Default
  new val create()

primitive A
primitive B

class Generic[T: (Default val | None val)]
  let x: T = T.create()
  fun get(): T => x

actor Main
  new create(env: Env) =>
    Generic[(A | B)].get()
```

```
src/libponyc/ast/error.c:73: ast_error_frame: Assertion `ast != NULL` failed.
```

The compiler now reports a proper error instead of crashing.
