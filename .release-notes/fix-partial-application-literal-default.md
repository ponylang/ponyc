## Fix compiler crash on partial application of a method with a literal default argument

Partially applying a method whose default argument is built from a numeric literal would crash the compiler. For example, this crashed during compilation:

```pony
use "format"

actor Main
  new create(env: Env) =>
    Format~apply()
```

`Format.apply` has a parameter whose default argument is `-1`, and partially applying the method triggered the crash. Any method with a comparable default (for instance `0 + 1`) was affected. These now compile and behave correctly.

Relatedly, partially applying a method whose default argument is itself invalid — such as `(-1).abs()`, where the literal has no type to look up `abs` on — now reports a normal compile error instead of crashing the compiler.
