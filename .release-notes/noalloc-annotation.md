## Add `\noalloc\` annotation for methods

The `\noalloc\` annotation marks a method whose body must not allocate heap memory, either directly or through calls to other functions. The compiler enforces this in several ways: a `\noalloc\` method cannot make a virtual call to a method that is not also `\noalloc\`, a class implementing a trait with `\noalloc\` methods must mark its implementations `\noalloc\` too, FFI calls and bare lambda calls are not allowed inside a `\noalloc\` method, and the LTO pass verifies at link time that the transitive call graph of a `\noalloc\` function contains no allocation call, no FFI call, and no indirect call through a bare lambda.

```pony
trait Computable
  fun \noalloc\ compute(x: U32): U32

class Calculator is Computable
  fun \noalloc\ compute(x: U32): U32 => x * 2
```

The annotation can appear on `fun`, `be`, and `new` declarations.
