## Fix missing question mark check for partial calls in trait default bodies

Calling a partial method without `?` inside a trait's default method body now correctly produces a compile error, matching the behavior of primitives and interfaces.

Previously, code like this compiled silently:

```pony
trait T
  fun f1() ? => error
  fun f2() ? => f1()    // missing `?`, but compiler did not complain
```

The same code in a primitive or interface correctly errored; only traits were affected.

If your code was relying on this missing check, add the `?` to the call site:

```pony
trait T
  fun f1() ? => error
  fun f2() ? => f1()?
```
