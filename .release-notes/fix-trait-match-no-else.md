## Fix match without else in trait default method always taking the first arm

A `match` without an explicit `else` in a trait default method body always took the first capture arm, regardless of the runtime type of the matched value. The first capture arm always ran, which could crash the program or silently produce wrong results when the matched value was a different type.

```pony
trait T
  fun get(): (R | None)

  fun check() =>
    match get()
    | let r: R => r.n  // always taken, even when get() returns None
    end
```

The same match written in a class body, or with an explicit `else`, worked correctly.
