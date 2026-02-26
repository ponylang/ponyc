## Add `\exhaustive\` annotation for match expressions

A new `\exhaustive\` annotation can be applied to match expressions to assert that all cases are explicitly handled. When present, the compiler will emit an error if the match is not exhaustive and no else clause is provided, instead of silently injecting an `else None`.

Without the annotation, a non-exhaustive match silently compiles with an implicit `else None`, which changes the result type to `(T | None)` and produces an indirect type error like "function body isn't the result type." With the annotation, the error message directly identifies the problem.

```pony
type Choices is (T1 | T2 | T3)

primitive Foo
  fun apply(p: Choices): String =>
    match \exhaustive\ p
    | T1 => "t1"
    | T2 => "t2"
    end
```

The above produces: `match marked \exhaustive\ is not exhaustive`

Adding the missing case or an explicit else clause resolves the error. The annotation also serves as a future-proofing assertion on already-exhaustive matches -- if a new member is added to the union type later, the compiler will flag the match as no longer covering all cases.
