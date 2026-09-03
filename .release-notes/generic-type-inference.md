## Add generic type argument inference for method and constructor calls

The compiler can now infer type arguments from the arguments of a generic call. When the arguments determine a unique type for each type parameter, you can omit the type arguments:

```pony
primitive Sorter
  fun sort[A: Comparable[A] val](a: A, b: A): (A, A) =>
    if a < b then (a, b) else (b, a) end

actor Main
  new create(env: Env) =>
    // Before: Sorter.sort[U8](U8(3), U8(1))
    // After:
    let result = Sorter.sort(U8(3), U8(1))
```

Constructor calls infer the class's type parameters the same way:

```pony
class Pair[A: Any val, B: Any val]
  let _a: A
  let _b: B
  new create(a: A, b: B) => _a = a; _b = b

actor Main
  new create(env: Env) =>
    // Before: Pair[String, U8]("hello", U8(42))
    // After:
    let p = Pair("hello", U8(42))
```

When a type parameter has a default, the default is kept when every argument fits it. Otherwise the inferred type replaces the default:

```pony
class Wrapper[A: Any val = String]
  let _a: A
  new create(a: A) => _a = a

actor Main
  new create(env: Env) =>
    let w1 = Wrapper("hello")   // A is String (the default fits)
    let w2 = Wrapper(U8(42))    // A is U8 (the argument overrides the default)
```

When a type parameter has a default that refers to another type parameter in the same list — `[A: Any val = B, B: Any val = U8]` — and the referred-to parameter cannot be determined from the arguments, the compiler reports which parameter's default it cannot resolve. Programs that write out their type arguments are not affected; programs where such a default caused a crash at code generation now get a compile-time error instead.

When a parameter type mentions a type parameter, arguments at that position whose own type depends on the inferred type — such as lambdas and array literals — are skipped during inference and typed afterward. This works as long as at least one other argument determines the type parameter:

```pony
primitive Bar
  fun foo[A: Any val](a: A, f: {(A): A} val): A =>
    f(a)

actor Main
  new create(env: Env) =>
    // A is inferred as U8 from the first argument;
    // the lambda is then typed against {(U8): U8} val
    let x: U8 = Bar.foo(U8(42), {(x: U8): U8 => x + 1})
```

Each inferred type argument is the type a `let` binding would receive from that argument position, except that an `iso` or `trn` alias is kept as `iso` or `trn` so the compiler can report a missing `consume` rather than silently widening.

For programs that already fail to compile, the first error reported — and sometimes the number of errors — can change.

### Known gaps

The following do not participate in type argument inference. Write the type arguments explicitly when you use them:

- Array literals and lambda arguments at positions where the parameter type mentions a type parameter through structural nesting. `Foo(["a"; "b"])` still needs `Foo[String](["a"; "b"])`.
- Type parameters that appear only in a lambda's result type.
- Type aliases wrapping a generic type.
- Union-typed parameters. A parameter like `(Array[A] val | Array[U8] val | None)` does not determine `A`; write the type argument explicitly.
- The `where` syntax for named-only arguments.
- A generic method called on a generic type written without its type arguments (`Foo.some_method(x)` where `Foo` has defaulted type parameters): the method's arguments are typed before inference runs, so array literals and lambdas at those positions see the unresolved parameter type. Adding explicit type arguments to either `Foo` or the method avoids this.
