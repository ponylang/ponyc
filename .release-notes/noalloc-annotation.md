## Add `\noalloc\` annotation for methods

The `\noalloc\` annotation marks a method whose body must not allocate heap memory, either directly or through calls to other functions. The compiler enforces this in three ways: a `\noalloc\` method cannot make a virtual call to a method that is not also `\noalloc\`, a class implementing an interface with `\noalloc\` methods must mark its implementations `\noalloc\` too, and the LTO pass verifies at link time that the transitive call graph of a `\noalloc\` function contains no allocation call.

```pony
interface Computable
  fun \noalloc\ compute(x: U32): U32

class Calculator is Computable
  fun \noalloc\ compute(x: U32): U32 => x * 2
```

The annotation can appear on `fun`, `be`, and `new` declarations.
