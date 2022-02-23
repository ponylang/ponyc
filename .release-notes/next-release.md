## Add workaround for compiler assertion failure in Promise.flatten_next

Under certain conditions a specific usage of `Promise.flatten_next` and `Promise.next` could trigger a compiler assertion, whose root-cause is still under investigation.

The workaround avoids the code-path that triggers the assertion. We still need a full solution.

## Take exhaustive match into account when checking for field initialization

Previously, exhaustive matches were handled after we verified that an object's fields were initialized. This lead to the code such as the following not passing compiler checks:

```pony
primitive Foo
  fun apply() : (U32 | None) => 1

type Bar is (U32 | None)

actor Main
  let bar : Bar

  new create(env : Env) =>
      match Foo()
      | let result : U32 =>
          bar = result
      | None =>
          bar = 0
      end
```

Field initialization is now done in a later pass after exhaustiveness checking has been done.

## Fix compiler crash related to using tuples as a generic constraint

Tuple types aren't legal constraints on generics in Pony and we have a check in an early compiler pass to detect and report that error. However, it was previously possible to "sneak" a tuple type as a generic constraint past the earlier check in the compiler by "smuggling" it in a type alias.

The type aliases aren't flattened into their various components until after the code that disallows tuples as generic constraints which would result in the following code causing ponyc to assert:

```pony
type Blocksize is (U8, U32)
class Block[T: Blocksize]
```

We've added an additional check to the compiler in a later pass to report an error on the "smuggled" tuple type as a constraint.

## Add greatest common divisor to `math` package

Code to determine the greatest common divisor for two integers has been added to the standard library:

```pony
use "math"

actor Main
  new create(env: Env) =>
    try
      let gcd = GreatestCommonDivisor[I64](10, 20)?
      env.out.print(gcd.string())
    else
      env.out.print("No GCD")
    end
```

## Add least common multiple to `math` package

Code to determine the least common multiple of two unsigned integers has been added to the standard library:

```pony
use "math"

actor Main
  new create(env: Env) =>
    try
      let lcm = LeastCommonMultiple[U64](10, 20)?
      env.out.print(lcm.string())
    else
      env.out.print("No LCM")
    end
```

## Fix incorrect "field not initialized" error with while/else

Previously, due to a bug in the compiler's while/else handling code, the following code would be rejected as not initializing all object fields:

```pony
actor Main
  var _s: (String | None)

  new create(env: Env) =>
    var i: USize = 0
    while i < 5 do
       _s = i.string()
       i = i + 1
    else
      _s = ""
    end
```

## Fix incorrect "field not initialized" error with try/else

Previously, due to a bug in the compiler's try/else handling code, the following code would be rejected as not initializing all object fields:

```pony
actor Main
  var _s: (String | None)

  new create(env: Env) =>
    try
      _s = "".usize()?.string()
    else
      _s = None
    end
```

## Fix compiler crash related to using tuples in a union as a generic constraint

Tuple types aren't legal constraints on generics in Pony and we have a check in an early compiler pass to detect and report that error. However, it was previously possible to "sneak" a tuple type as a generic constraint past the earlier check in the compiler by "smuggling" it in a type union in a type alias.

The type aliases aren't flattened into their various components until after the code that disallows tuples as generic constraints which would result in the following code causing ponyc to assert:

```pony
type Blocksize is (U64 | (U8, U32))
class Block[T: Blocksize]
```

We've added an additional check to the compiler in a later pass to report an error on the "smuggled" tuple type as a constraint.

## Fix incorrect code returned by `ANSI.erase`

`erase` was intended to erase all characters to the left of the cursor but was instead returning the code to erase all characters to the right.

## Reimplement `with`

The `with` keyword has been little used in most Pony programs. `with` was implemented as "sugar" over the `try/else/then` construct. Over the course of time, this became problematic as changes were made to make `try` more friendly. However, these `try` changes negatively impacted `with`. Prior to this change, `with` had become [barely usable](https://github.com/ponylang/ponyc/issues/3759). We've reimplemented `with` to address the usability problems that built up over time. `with` is no longer sugar over `try` and as such, shouldn't develop any unrelated problems going forward. However, the reimplemented `with` is a breaking change.

Because `with` was sugar over `try`, the full expression was `with/else`. Any error that occurred within a `with` block would be handled by provided `else`. The existence of `with/else` rather than pure `with` was not a principled choice. The `else` only existed because it was needed to satisfy error-handling in the `try` based implementation. Our new implementation of `with` does not have the optional `else` clause. All error handling is in the hands of the programmer like it would be with any other Pony construct.

Previously, you would have had:

```pony
with obj = SomeObjectThatNeedsDisposing() do
  // use obj
else
  // only run if an error has occurred
end
```

Now, you would do:

```pony
try
  with obj = SomeObjectThatNeedsDisposing() do
    // use obj
  end
else
  // only run if an error has occurred
end
```

Or perhaps:

```pony
with obj = SomeObjectThatNeedsDisposing() do
  try
    // use obj
  else
    // only run if an error has occurred
  end
end
```

The new `with` block guarantees that `dispose` will be called on all `with` condition variables before jumping away whether due to an `error` or a control flow structure such as `return`.

This first version of the "new `with`" maintains one weakness that the previous implementation suffered from; you can't use an `iso` variable as a `with` condition. The following code will not compile:

```pony
use @pony_exitcode[None](code: I32)

class Disposable
  var _exit_code: I32

  new iso create() =>
    _exit_code = 0

  fun ref set_exit(code: I32) =>
    _exit_code = code

  fun dispose() =>
    @pony_exitcode(_exit_code)

actor Main
  new create(env: Env) =>
    with d = Disposable do
      d.set_exit(10)
    end
```

A future improvement to the `with` implementation will allow the usage of `iso` variables as `with` conditions.

## Fix the interfaces for Iter's `map` and `map_stateful` methods to be less strict

The `map_stateful` and `map` methods on an Iter (from the itertools package) were too strict in what they required, requiring a `B^` in order to produce an `Iter[B]`.

This change correctly makes them only require `B`.

## Use symbol table from definition scope when looking up references from default method bodies

Previously, symbols from default method bodies were only looked up in the local scope of the file into which they were included. This resulted in compilation errors if said symbol wasn't available in the local scope.

Issues [#3737](https://github.com/ponylang/ponyc/issues/3737) and [#2150](https://github.com/ponylang/ponyc/issues/2150) were both examples of this problem.

We've updated definition lookup to check if the enclosing method is provided by a trait or interface and if it is, to also check to definition scope for any needed symbols.

## Remove `logger` package from the standard library

The `logger` package is intentionally limited in its functionality and leans heavily into reducing messaging overhead between actors to the point of intentionally sacrificing functionality.

Our stated belief as a core team has been that the standard library isn't "batteries included". We have also stated that libraries were we believe it would be "relatively easy" for multiple competing standards to appear in the community shouldn't be included in the standard library unless other factors make inclusion important.

Some other factor we have discussed in the past include:

- Not having a standard library version would make interop between different 3rd-party Pony libraries difficult.
- The functionality is required or called for by an interface in one or more standard library classes.
- We consider the having the functionality provided to be core to the "getting started with Pony experience".

We don't believe that any of above 3 points apply to the `logger` package. Therefore, we've removed `logger` from the standard library and turned it into it's own [stand alone library](https://github.com/ponylang/logger).

If you were using the `logger` package, visit it's new home and follow the "How to Use" directions.

## Fix call receiver type casting in LLVM IR

In some rare cases (such as in the PonyCheck library), `ponyc` was generating LLVM that did not use the correct pointer types for call site receiver arguments. This did not result in unsafe code at runtime, but it was still a violation of the LLVM type system, and it caused the "Verification" step of compilation to fail when enabled.

To fix this issue, we introduced an LLVM bitcast at such call sites to cast the receiver pointer to the correct LLVM type.

## Rename `ponybench` package to match standard library naming conventions

We recently realized that when we renamed large portions of the standard library to conform with our naming standards, that we missed the `ponybench` package. To conform with the naming convention, the `ponybench` package as been renamed to `pony_bench`.

You'll need to update your test code from:

```pony
use "ponybench"
```

to

```pony
use "pony_bench"
```

