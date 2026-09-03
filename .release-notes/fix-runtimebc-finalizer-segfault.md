## Fix segfault in programs with finalizers compiled with --runtimebc

Programs compiled with `--runtimebc` that used classes with `_final()` methods crashed with a segfault during garbage collection.

```pony
class Foo
  fun _final() =>
    None

actor Main
  new create(env: Env) =>
    Foo
```

The same program ran correctly without `--runtimebc`. This has been fixed.
