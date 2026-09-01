## Fix false "declaration appears after use" error with trait default method bodies

When a type implemented multiple traits that declared the same method, and one of those traits provided a default body, the compiler could reject the program with a false "declaration of 'env' appears after use" error:

```pony
trait Concrete
  fun hello(env: Env) =>
    env.out.print("hello")

trait Abstract1
  fun hello(env: Env)

actor Main is (Abstract1 & Concrete)
  new create(env: Env) =>
    hello(env)
```

Whether the compiler raised the error depended on the order the traits were declared in source. This has been fixed.

## Fix false "this pattern can never match" error for a type parameter inside a type argument

Matching a generic value against a pattern whose type argument contained a type parameter — for example, `Cons[List[B]]` against a value of `Cons[A]` — was rejected at compile time with "this pattern can never match" even when `A` could reify to `List[B]` at runtime:

```pony
class val Cons[A: Any #share]
  let head: A
  let tail: (Cons[A] | Nil[A])
  new val create(h: A, t: (Cons[A] | Nil[A])) =>
    head = h
    tail = t

primitive Nil[A: Any #share]

type List[A: Any #share] is (Cons[A] | Nil[A])

primitive PickHead
  fun apply[B: Any #share](xs: List[List[B]]): (List[B] | None) =>
    match xs
    | let c: Cons[List[B]] => c.head
    else
      None
    end
```

```
Error:
main.pony:14:7: this pattern can never match
    | let c: Cons[List[B]] => c.head
      ^
```

Class, actor, and primitive patterns of this shape are now accepted; the match uses the fully reified type at runtime as it does for any other pattern.

## Fix false "this pattern can never match" error for a type parameter inside a trait or interface type argument

Matching a class value against a trait or interface pattern whose type argument contained a type parameter — for example, `T[Wrap[B] val]` against a value of `Cons[A]` where `Cons` provides `T` — was rejected at compile time with "this pattern can never match" even when `A` could reify to `Wrap[B] val` at runtime:

```pony
class val Wrap[A: Any #share]
  let value: A
  new val create(v: A) => value = v

trait val T[A: Any #share]
  fun get_value(): A

class val Cons[A: Any #share] is T[A]
  let _v: A
  new val create(v: A) => _v = v
  fun get_value(): A => _v

primitive Check
  fun apply[A: Any #share, B: Any #share](x: Cons[A]) =>
    match x
    | let _: T[Wrap[B] val] val => None
    else
      None
    end
```

```
Error:
main.pony:16:7: this pattern can never match
    | let _: T[Wrap[B] val] val => None
      ^
```

Trait and interface patterns of this shape are now accepted when the operand's class provides the pattern's trait or interface; the match uses the fully reified type at runtime as it does for any other pattern.

## Fix false "this pattern can never match" error for constraint-overlapping type parameters and union, intersection, or tuple type arguments

Three more shapes where a match with generic type arguments used to compile-error with "this pattern can never match" now compile and match at runtime when the reified types coincide.

The first is a pair of type parameters whose constraints don't relate by subtyping but share a common inhabitant. Neither `(P1 | P2)` nor `(P2 | P3)` is a subtype of the other, but both admit `P2`, so a reification `A = B = P2` makes the two `Cell` types equal:

```pony
primitive P1
primitive P2
primitive P3

class val Cell[A: Any #share]
  let value: A
  new val create(v: A) => value = v

primitive Check
  fun apply[A: (P1 | P2), B: (P2 | P3)](x: Cell[A]) =>
    match x
    | let _: Cell[B] => None
    else
      None
    end
```

The second is a type argument that is a union or intersection literal containing a type parameter. Here `A` reifies to `U8` so the operand and pattern share a runtime descriptor:

```pony
primitive P

class val Cell[A: Any #share]
  let value: A
  new val create(v: A) => value = v

primitive Check
  fun apply[A: Any #share](x: Cell[(P | A)]) =>
    match x
    | let _: Cell[(P | U8)] => None
    else
      None
    end
```

The third is a tuple type argument containing a type parameter, handled the same way — element by element:

```pony
primitive Check2
  fun apply[A: Any #share](x: Cell[(P, A)]) =>
    match x
    | let _: Cell[(P, U8)] => None
    else
      None
    end
```

Matches where no reification could ever satisfy the type-argument pair still reject at compile time.

## Fix `Vec.slice` and `Vec.reverse` in `collections/persistent`

`Vec.slice` always returned an empty vector, whatever range it was given, and it ignored its `from` argument:

```pony
let v = Vec[USize].concat(Range(0, 10))
v.slice(2, 5).size() // was 0, is now 3
```

`Vec.reverse` never returned when called on an empty vector; it looped indefinitely instead.

Both have been fixed. `slice` now returns the requested range, saturating `to` at the size of the vector, and `reverse` on an empty vector returns an empty vector.

## Add --pass-timings/--pass-timings-json for profiling compiler pass times

`ponyc` can now report how long each front-end pass takes on each package, to help you find the slow part of a slow build.

`--pass-timings` prints one table to stderr after a build, with a row per package and pass and its wall, user, and system time. A row like `mylib/thing (expr)` is the time type checking spent on that package, so you can see which pass on which package is slow rather than only that the build is slow overall.

Pass `--pass-timings-json=FILE` to write the same timings as JSON for scripting or tracking over time; on its own it writes only the file, so combine it with `--pass-timings` if you also want the table on stderr. Each JSON file also records whether the build succeeded, the compiler version and target triple, and the total elapsed time, so a stored file describes the build it came from.

Only the front-end passes are timed. Compiling C shims, plugin passes, reach, codegen, LLVM optimisation and linking are not, so the rows can account for a small share of a long build. The table prints the elapsed wall-clock time alongside the rows so you can see what share they cover.

Times are inclusive, so rows can overlap: loading a package runs its parse and syntax inside the importing package's scope row, and that time is counted in both. Time spent instantiating a generic is counted in the row for the pass that triggered it, so a package's `expr` row includes the earlier passes re-run on its instantiations.

```
ponyc --pass-timings my_package
ponyc --pass-timings --pass-timings-json=timings.json my_package
```

Compiling several packages in one `ponyc` invocation writes a single report covering all of them, with rows summed across them.

The JSON file is written when the build ends, so a build you interrupt produces no timings and leaves any file from an earlier run untouched.

## Fix false unreachable match for structural interface patterns

The fix for trait and interface match patterns with a type parameter inside a type argument only handled classes that nominally provide the interface via `is I[A]`. A class that structurally satisfies the interface — same methods, same signatures, no `is` declaration — still produced the false error:

```pony
class val Wrap[A: Any #share]
  let _value: A
  new val create(v: A) => _value = v
  fun value(): A => _value

interface val I[A: Any #share]
  fun value(): A

class val Cons[A: Any #share]
  let _v: A
  new val create(v: A) => _v = v
  fun value(): A => _v

primitive Check
  fun apply[A: Any #share, B: Any #share](x: Cons[A]): I32 =>
    match x
    | let _: I[Wrap[B] val] val => 1
    else
      2
    end
```

```
Error:
main.pony:17:7: this pattern can never match
    | let _: I[Wrap[B] val] val => 1
      ^
```

Structural interface patterns with type parameters in type arguments are now accepted. As with the nominal case, the match uses the fully reified type at runtime.

## Fix false unreachable match for structural interface patterns with lambda parameters

Matching a class against a structural interface pattern produced a false "this pattern can never match" error when the class and the interface used structurally equivalent but nominally different single-method interface types in their method parameters. The most common case is a method with a lambda parameter type (`{(A): B} val`) matched against a pattern whose interface uses either a lambda written at a different source position or a user-defined interface with the same method signature:

```pony
class val Wrap[A: Any #share]
  let _value: A
  new val create(v: A) => _value = v
  fun value(): A => _value

interface val Mapper[A: Any #share]
  fun map_it[B: Any #share](f: {(A): B} val): B

class val Box[A: Any #share]
  let _v: A
  new val create(v: A) => _v = v
  fun map_it[B: Any #share](f: {(A): B} val): B => f(_v)

primitive Check
  fun apply[A: Any #share, B: Any #share](x: Box[A]): I32 =>
    match x
    | let _: Mapper[Wrap[B] val] val => 42
    else
      1
    end
```

```
Error:
main.pony:17:7: this pattern can never match
    | let _: Mapper[Wrap[B] val] val => 42
      ^
```

Single-method interfaces with different definitions but identical method signatures are now compared structurally rather than by identity.

## Fix deferred stdout output on Windows

On Windows, stdout and stderr were left at the C runtime's default full buffering. Output from `env.out` could sit in the buffer and not appear until the program exited, especially when the program spent time in blocking FFI calls between prints. The same buffering was already configured on Unix (unbuffered for a terminal, line-buffered otherwise) but the Windows path was missing it.

## Fix false "unreachable code" error when all branches jump away in a None-returning function

Previously, a function returning `None` where every branch of a control construct jumped away (via `return`, `error`, `break`, or `continue`) was rejected with a false "unreachable code" error:

```pony
actor Main
  fun maybe_error(dont_err: Bool = false) ? =>
    if dont_err then
      return
    else
      error
    end

  new create(env: Env) =>
    try maybe_error()? end
```

```
Error:
main.pony:8:5: unreachable code
```

This affected `if`, `iftype`, `while`, `repeat`, `try`, `match`, and `with` constructs. The error pointed at compiler-generated code the user never wrote. This has been fixed.

## Fix `repeat` loop field initialization tracking with `break`

The compiler rejected valid programs where a `repeat` loop body initialized a field before an unconditional `break`. The `else` clause was required to also initialize the field, even though the body always did so before exiting. This program now compiles:

```pony
actor Main
  var _s: (String | None)
  new create(env: Env) =>
    repeat
      _s = None
      break
    until true else
      None
    end
```

## Fix crash when compiling with --runtimebc

Programs compiled with `--runtimebc` could crash at runtime with a segfault. The runtime bitcode was being built with debug-mode settings instead of release-mode, and with a mismatched target triple that produced a linker warning on every `--runtimebc` compile.

Both are fixed.

## Fix compiler crash from return inside resolved iftype branch

Using `return` inside an `iftype` branch caused the compiler to crash:

```pony
primitive Foo[A: Seq[B] ref, B: Comparable[B] #read]
  fun apply(a: A) =>
    iftype A <: Array[B] then
      return None
    end
    None
```

```
LLVM ERROR: Broken module found, compilation aborted!
```

The same crash occurred when the `iftype` was inside a `recover` block.

This has been fixed.

## Fix overly strict consumed-variable check in try/else blocks

Previously, consuming a variable after the last error point in a `try` block made the variable unusable in the `else` block, even though the consume could not have executed on any path that reaches `else`:

```pony
actor Main
  new create(env: Env) =>
    try
      partial()?
      consume env  // only runs if partial() succeeded
    else
      consume env  // error: can't use a consumed local
    end

  fun partial() ? => error
```

The example above now compiles correctly.

Variables consumed before or at an error point are still correctly rejected in `else`.

## Fix destructuring assignment for unions of same-arity tuples

Destructuring a union of tuples via assignment now works when every member of the union is a tuple with the same arity. Previously the compiler rejected it with "can't destructure a union using assignment, use pattern matching instead," even though the destructuring is type-safe. The most common trigger was iterating an array of lambda tuples without an explicit `as` type:

```pony
let handlers = [({(s: String): String => s + "!" },
                  {(s: String): String => s + "?" })
                ({(s: String): String => s + "." },
                  {(s: String): String => s + "," })]

for (exclaim, question) in handlers.values() do
  env.out.print(exclaim("hi") + question("hi"))
end
```

The workaround was to add an `as` clause to the array literal specifying the structural type. That is no longer necessary.

## Fix `style/blank-lines` false positive on block comments between declarations

`pony-lint`'s `style/blank-lines` rule fired on block comments placed between type declarations when the comment contained blank lines (paragraph breaks). No arrangement of blank lines around the comment satisfied the rule:

```pony
class val Foo
  let x: U8
  new val create(x': U8) => x = x'

/*
Comment with

a paragraph break.
*/
type Bar is (Foo | None)
```

Blank lines inside `/* */` comments are now excluded from the between-entities count. A blank line before the comment still satisfies the one-blank-line requirement between declarations.

## Add multi-iterator for loop sugar

Pony's `for` loop now accepts multiple iterators, zipping them together:

```pony
for (a, b) in (iter_a, iter_b) do
  env.out.print(a.string() + " " + b.string())
end
```

The loop runs until the shortest iterator is exhausted. Three binding forms are supported: destructured names matching the iterator count, nested destructuring for iterators that yield tuples, and a single name that receives the full tuple of values.

## Type parameter constraints now respect the default cap of the named type

When a type name appeared in a type parameter constraint without an explicit capability, the compiler substituted `#any` instead of using the type's declared default capability (`ref` for classes and interfaces, `val` for primitives, `tag` for actors). This was the only context in the language where default capabilities were ignored.

```pony
class Foo
  fun bar() => None

// Before: A: Foo meant A: Foo #any
// After:  A: Foo means A: Foo ref (the default cap of class Foo)
fun example[A: Foo](x: A) => None
```

Code that relied on the implicit `#any` needs an explicit capability. The most common case is intersection constraints where one member had an explicit cap and the other did not:

```pony
// Before (compiled because Stringable got #any):
fun test_sort[A: (Comparable[A] val & Stringable)](x: A) => None

// After (Stringable needs an explicit val to match):
fun test_sort[A: (Comparable[A] val & Stringable val)](x: A) => None
```

The same applies to `iftype` conditions. If a type parameter is constrained to a `val` capability and the `iftype` supertype is an interface, that interface now gets its default `ref` cap, making the condition unsatisfiable. Add an explicit cap that matches:

```pony
// Before (HasDocs got #any, so the condition was satisfiable):
fun foo[A: AST val](node: A) =>
  iftype A <: HasDocs then
    node.docs()
  end

// After (HasDocs needs val to be compatible with the constraint):
fun foo[A: AST val](node: A) =>
  iftype A <: HasDocs val then
    node.docs()
  end
```

## Fix persistent Map iterators reporting items on an empty map

`keys`, `values` and `pairs` on an empty `collections/persistent` `Map`, and `values` on an empty `Set`, returned an iterator whose `has_next` was `true` while `next` raised. A `for` loop over one of these was unaffected, but code that drives the iterator directly, or passes it to something that does, saw an iterator that never reported being exhausted.

```pony
let m = Map[String, U32]
m.pairs().has_next() // returned true, now returns false
```

## Fix persistent Map removing the wrong entry for a key it doesn't contain

Removing a key that isn't in a `collections/persistent` `Map` could delete a different entry instead of raising an error. The returned map was missing an entry the caller never named, and `size` was decremented to match, so nothing downstream flagged the loss.

This affected `remove`, `sub` and the `-` operator, and `Set` through the same operators. `sub` and `-` gave no signal at all, because they return a map rather than raising.

`Set.without` and `Set.op_xor` reach the same path internally, so a set could lose an element without the calling code removing anything: an argument that yields the same element twice was enough, because both test membership against the receiver while subtracting from an accumulator.

`json` is built on this map, so `JSONObject.remove` and `JSONLens.remove` were affected too: removing a key or a path the document doesn't contain could delete a different one, despite `JSONObject.remove` documenting itself as a no-op in that case.

```pony
let m = Map[String, U32] // {}
let m2 = m("a") = 5      // {a: 5}
let m3 = m2 - "2"        // returned {} instead of {a: 5}
```

`remove` now raises for any key the map doesn't contain, and `sub` and `-` return the map unchanged.

## Add `\c_api\` annotation for C-ABI interop (experimental)

This feature is experimental and may change in future releases.

The `\c_api\` annotation on a class, primitive, struct, or actor exposes its public methods to C callers. The compiler generates C-ABI wrapper functions and a `.h` header that C shim files can include.

```pony
class \c_api\ val Adder
  let _base: I64
  new val create(base: I64) => _base = base
  fun val add(x: I64): I64 => _base + x
```

The `use` alias in the consuming package determines C-facing names. With `use "mylib"`, the wrapper is `mylib_Adder_add` and the header is `mylib_export.h`. With `use math = "mylib"`, they become `math_Adder_add` and `math_export.h`. In the main package, names have no prefix.

```c
#include "mylib_export.h"

int64_t add_from_c(void* adder, int64_t x) {
  return mylib_Adder_add(adder, x);
}
```

Exported primitive methods omit the `self` parameter — primitives are stateless, so the C caller doesn't need to pass a receiver.

To export a concrete reification of a generic type, annotate a type alias:

```pony
class MyBox[A]
  let _value: A
  new val create(value: A) => _value = value
  fun val get(): A => _value

type \c_api\ BoxedI64 is MyBox[I64]
```

Constructors, behaviors, private methods, partial methods, and methods with tuple parameters or return types are excluded from export. It is an error to annotate a type whose methods are all excluded. Generic types cannot be exported directly; use a type alias to export a concrete reification.

## Single-subtype devirtualization for interface/trait dispatch

Calling a method through an interface or trait now uses a direct call instead of a vtable lookup when the program has exactly one concrete type implementing that interface. LLVM can then inline the direct call and optimize across the call boundary.

This matters most for code that passes closures or iterators through generic combinators like `Iter.fold` — the lambda and iterator calls that were previously indirect become direct calls, eligible for inlining.

## Fix data loss in `collections/persistent` `Vec.remove`

`Vec.remove(i, n)` destroyed elements outside the requested range whenever fewer than `n` elements followed index `i`.

```pony
use "collections/persistent"

actor Main
  new create(env: Env) =>
    try
      let v = Vec[USize].concat([as USize: 0; 1; 2; 3; 4].values())

      // specifies removal of index 4 and two indices that do not exist
      let r = v.remove(4, 3)?

      // elements 2 and 3 were live, were not named, and are gone
      for x in r.values() do env.out.write(x.string() + " ") end
    end
```

```
0 1
```

When fewer than `n` elements followed index `i`, elements before `i` were also destroyed. No error was raised, and `size` was reduced to match the shortened vector, so nothing a caller could inspect revealed the loss.

This has been fixed. The count is now saturated: if fewer than `n` elements follow `i`, every element from `i` onward is removed and nothing before `i` is touched. The example above now prints `0 1 2 3`. This matches `Array.remove`, which `Vec.remove` mirrors, and `Vec.slice`, which already documented a saturated range. An index `i` that is out of bounds still raises an error.

## Fix object literal compilation with union-constrained type parameters

Object literals inside methods whose type parameters have union constraints with mixed capabilities failed to compile:

```pony
interface box FnBox[A, B]
  fun apply(a: A): B ?
interface ref FnRef[A, B]
  fun ref apply(a: A): B ?
type Fn[A, B] is (FnBox[A, B] box | FnRef[A, B] ref)

actor Main
  new create(env: Env) => None
  fun bar[A, B, F: Fn[A, B]](f: F) =>
    object ref
      fun ref foo(a: A) ? =>
        iftype F <: FnBox[A, B] box then f(consume a)?
        elseif F <: FnRef[A, B] ref then f(consume a)?
        else error
        end
    end
```

The compiler reported "type argument is outside its constraint" for the object literal's captured type parameters. The same code without the object literal compiled correctly.

This has been fixed. Object literals inside methods with union-constrained type parameters now compile correctly.

## Improve heap-to-stack promotion for stored objects

The heap-to-stack optimization pass now promotes objects that are stored into fields of already-stack-promoted parents. Previously, storing a `pony_alloc`'d pointer into any field caused the pass to treat the allocation as escaped, keeping it on the heap even when the parent was local and never left the function.

A common case this unlocks: a local `String` whose backing buffer is a separate allocation stored into a String field. When the String is promoted to the stack, the buffer can now follow it.

## Fix iftype narrowing for methods that use `this->` viewpoint

Calling a method whose return type uses `this->` (such as `Array.clone`) on a `#send`-constrained type parameter narrowed via `iftype` produced a spurious "no lower bounds" error:

```pony
class iso T[A: Array[I64] #send]
  var data: A

  new iso create() =>
    data = recover Array[I64] end

  fun ref push(v: I64) =>
    iftype A <: Array[I64] val then
      data = recover data.clone() .> push(v) end
    end
```

```
Error:
main.pony:11:43: argument not assignable to parameter
      data = recover data.clone() .> push(v) end
                                          ^
    Info:
    I64 val is not a subtype of Array[I64 val] #send->I64 val^:
      the supertype has no lower bounds
```

The compiler now uses the narrowed capability when computing `this->` viewpoint types inside `iftype` branches.

## Fix tuple subtyping for tuples with tag elements

Tuples containing `tag` elements were rejected where `#read` or `val` capabilities were required. The most common trigger was a type alias like `type Rec is (USize, Actr tag)` used with a generic constrained to `Any #read`, such as a priority queue. The compiler applied the outer capability to every tuple element, which produced an impossible type when an element was already constrained — actors, for example, are always `tag`. The tuple's capability is now derived from its element capabilities, and these cases compile as expected.

```pony
actor Actr
type Rec is (USize, Actr tag)

class PQ[T: Any #read]
  fun ref insert(value: T) => None

actor Main
  new create(env: Env) =>
    let pq = PQ[Rec]
    pq.insert((1, Actr))
```

## Applying a capability to a tuple type alias is a compile error

A tuple's capability is derived from its element capabilities. Writing `FooPair val` where `type FooPair is (Foo, Foo)` now produces a compile error. Specify capabilities on each element directly.

```pony
// Before: the capability was distributed to each element
type FooPair is (Foo, Foo)
let x: FooPair val = ...

// After: specify capabilities on the elements
type FooPair is (Foo val, Foo val)
let x: FooPair = ...
```

## Fix compiler accepting `?` functions whose only error source is self-recursion

A function marked `?` compiled successfully when its only source of partiality was a recursive call to itself. The partiality was circular — the call raises because the function is partial, and the function is partial because of the call — so no error could ever be raised:

```pony
primitive Foo
  fun apply(x: Bool): Bool ? =>
    apply(not x)?
```

The compiler now rejects this with "function signature is marked as partial but the function body cannot raise an error."

Mutual recursion — A calls B, B calls A, neither with an independent error source — is not yet detected.

## Fix wrapping division of signed minimum value by -1

Dividing a signed integer's minimum value by -1 with the wrapping `/` operator returned 0 instead of wrapping to the minimum value. The same was true of floored division (`fld`). For example:

```pony
let x = I64.min_value() / -1  // was 0, now -9223372036854775808
let y = I64.min_value().fld(-1)  // also now -9223372036854775808
```

The mathematical result of `MIN_VALUE / -1` is one past `MAX_VALUE`, so wrapping arithmetic should produce `MIN_VALUE` — the same way `MAX_VALUE + 1` wraps to `MIN_VALUE`. This now works correctly for all signed integer types, including `I128`.

Using literal constants (e.g., `I8(-128) / I8(-1)`) previously produced a spurious compile error ("constant divide or rem overflow") instead of folding to the correct value. That is also fixed.

The checked variants (`divc`, `fldc`) now return the correct wrapped value alongside the overflow flag, rather than returning 0 with the flag set.

Remainder (`%`, `%%`) was not affected — it already returned 0, which is the correct result.

## Fix I128 and U128 divrem_unsafe on native128 platforms

`I128.divrem_unsafe` and `U128.divrem_unsafe` returned the product and quotient instead of the quotient and remainder on platforms with native 128-bit integer support (64-bit Linux, macOS, and BSD). For example, `U128(10).divrem_unsafe(U128(3))` returned `(30, 3)` instead of `(3, 1)`.

The non-native128 fallback (Windows MSVC and 32-bit platforms) was not affected.

## Fix incorrect `#read->trn` viewpoint adaptation bounds

The compiler computed wrong upper and lower bounds when adapting `trn` through the `#read` generic capability. The upper bound was `box` instead of `trn`, and the lower bound was `trn` instead of `box`. This affected type checking of `this->trn` fields in classes and actors whose receiver capability is generic, and any other code path where the compiler needs the bounds of `#read->trn`.

## Fix stack overflow (SEGV) in persistent List from unbounded recursion

Most persistent `List` operations caused a stack overflow at ordinary list sizes.

```pony
use "collections/persistent"
use mut = "collections"

actor Main
  new create(env: Env) =>
    var l: List[USize] = Lists[USize].empty()
    for i in mut.Range(0, 50_000) do l = l.prepend(i) end
    env.out.print(l.map[USize]({(x) => x * 2 }).size().string())
```

```
Segmentation fault (core dumped)
```

The length that crashed depended on the stack size and the build, so the same program could work on one machine and crash on another. In a debug build with an 8 MB stack, `map`, `filter`, and `concat` crashed at around 50,000 elements; `apply` and `Lists.eq` at around 200,000 even in a release build.

This has been fixed. The example above now prints `50000`. No signatures changed.

## Fix incorrect viewpoint adaptation bounds for generic capability parameters

When accessing a field through a `trn` reference, and the field's type used a generic capability constraint (`#read`, `#alias`, or `#any`), the compiler computed incorrect capability bounds for the viewpoint-adapted type. Programs with unsound capability usage in these combinations could pass type checking without error.

Programs that relied on the incorrect bounds will now be correctly rejected by the compiler.

## Fix pony-doc displaying default parameter values

pony-doc displayed internal compiler token names like "call" and "reference" instead of the actual default parameter values written in source. A parameter declared as `fun apply(n: ISize = ISize.max_value())` appeared in the generated documentation with a default of "call" instead of `ISize.max_value()`. Similarly, `-1` appeared as "prefix".

Default values are now extracted from the original source text, so the documentation shows exactly what the user wrote.

## Fix incorrect rounding of float literals near the extremes of F64's range

Float literals near the extremes of F64's representable range were silently rounded to the wrong value. The most visible case: `2.2250738585072014e-308` (the smallest normal F64) compiled to zero.

```pony
// Before: compiled to 0x0 (zero)
// After:  compiled to 0x0010000000000000 (correct)
let x: F64 = 2.2250738585072014e-308
```

Float literal conversion is now correctly rounded per IEEE 754. A float literal and its string-parsed equivalent (via `String.f64()`) now produce the same bits.

