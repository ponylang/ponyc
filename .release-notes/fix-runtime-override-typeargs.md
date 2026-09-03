## Fix compiler crash on generic calls in runtime_override_defaults

Calling a primitive's function with type arguments inside `runtime_override_defaults` crashed the compiler with an assertion failure instead of compiling the program.

```pony
primitive P
  fun @get[B: Any val](b: B): U32 => 4

actor Main
  new create(env: Env) => None
  fun @runtime_override_defaults(rto: RuntimeOptions) =>
    rto.ponymaxthreads = P.get[U8](U8(1))
```

The same crash occurred with constructor calls that supply type arguments, such as `P.create[U8](U8(1)).get()`. Generic calls on primitives in `runtime_override_defaults` now compile as expected.
