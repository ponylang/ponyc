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

## Fix lambda type inference failure with generic #share constraints

A generic class with a `#share`-constrained type parameter that passed a lambda with inferred parameter types to a generic method like `Iter.map` would fail to compile with "the type parameter has no lower bounds." Explicitly annotating the lambda's parameter types worked around the issue. Inferred parameter types now compile correctly.

## Fix overly conservative viewpoint adaptation bounds for ephemeral generic capabilities

When reading a field with a generic capability constraint (`#read`, `#alias`, or `#any`) through an ephemeral origin (`iso^` or `trn^`), the compiler produced overly conservative type bounds. This could reject valid programs or assign less capable types than the soundness criterion permits.

The compiler now returns the tightest bound the formal criterion validates. For example, reading a `#read` field through an `iso^` origin produces a `val` upper bound instead of `tag`.

## Fix soundness bug with aliased type parameter constraints

When a type parameter's constraint used `!` on another type parameter, the compiler ignored the `!` modifier. This was a soundness hole: the compiler accepted code that violated reference capability guarantees.

The `!` modifier aliases a capability — `iso` becomes `tag`, `trn` becomes `box`. In this example, X should be constrained to `A tag` (because `Y!` where `Y: A iso` means "the aliased form of `A iso`," which is `A tag`). Before this fix, the compiler treated X as `A iso`, allowing code like this to compile:

```pony
class A
  var data: String = "hello"

actor Main
  new create(env: Env) =>
    let opaque: A tag = A
    // This should not compile — opaque is tag, not iso
    let stolen: A iso = reveal[A tag, A iso](opaque)

  fun reveal[X: Y!, Y: A iso](x: X): A iso^ =>
    consume x
```

The compiler now correctly rejects this code because `consume x` produces `A tag^`, not `A iso^`.

If your code stops compiling after this fix, look for type parameters constrained with `!` through another type parameter — the `!` is now enforced, so the constraint is tighter than it was before. The compiler error will show the actual capability. Adjust your code to work with the aliased capability (e.g. `tag` instead of `iso`, `box` instead of `trn`).

## Fix lambda capture types for fields accessed through non-ref receivers

Capturing a field in a lambda or object literal inside a method with a non-`ref` receiver (most commonly the default `box` receiver of `fun`) produced a spurious type error. The compiler typed the capture with the field's declared capability instead of adapting it through the method's receiver, so a `ref` field captured in a `fun box` method was typed as `ref` when it should have been `box`.

Before this fix, the workaround was to specify the capture type explicitly:

```pony
class Foo
  let _data: Array[U64]
  new create() => _data = Array[U64]

  fun box example() =>
    // This failed: "box is not a subtype of ref"
    {()(_data) => _data.size() }

    // Workaround: specify the type manually
    {()(x: Array[U64] box = _data) => x.size() }
```

Both the explicit capture syntax (`{()(field_name) => ...}`) and implicit captures in object literals are fixed.

## Fix iftype branches not satisfying type parameter return types

Concrete return values inside iftype branches were rejected with "no lower bounds" when the function's return type was a type parameter whose constraint is a union type, even when each branch returned the correct type for its narrowed constraint.

```pony
type FooBar is (Foo | Bar)

primitive Helper
  fun test[J: FooBar val](): J ? =>
    iftype J <: Foo val then recover Foo end  // was rejected
    elseif J <: Bar val then recover Bar end
    else error
    end
```

This code now compiles and runs correctly.

## Fix misleading error for bare lambda captures in tag methods

Capturing a field by name in a lambda inside a `fun tag` method reported the wrong receiver in the error message:

```pony
class Holder
  let _data: String ref
  new create() => _data = String

  fun tag example() =>
    {()(_data) => None }
```

```
can't read a field through {()} ref
```

The error now correctly names the enclosing type:

```
can't read a field through Holder tag
```

The equivalent explicit capture form (`{()(x = _data) => None}`) already reported the correct error. Both forms now produce the same message.

## Fix compiler crash when instantiating a generic with a union type argument that calls a constructor

Previously, instantiating a generic class that calls `T.create()` with a union type argument crashed the compiler with an assertion failure:

```pony
interface val Default
  new val create()

primitive A
primitive B

class Generic[T: (Default val | None val)]
  let x: T = T.create()
  fun get(): T => x

actor Main
  new create(env: Env) =>
    Generic[(A | B)].get()
```

```
src/libponyc/ast/error.c:73: ast_error_frame: Assertion `ast != NULL` failed.
```

The compiler now reports a proper error instead of crashing.

## Fix lambda and object literal captures inside iftype bodies losing type parameter narrowing

Inside an `iftype` body, capturing a field or local in a lambda or object literal lost the narrowed type constraint. Methods available through the narrowing failed to compile, even though direct references to the same variable worked:

```pony
class Wrapper[A: Any val]
  let _value: A

  fun example() =>
    iftype A <: Stringable val then
      _value.string()                         // worked
      let f = {()(_value) => _value.string()} // "couldn't find 'string' in 'Any val'"
    end
```

The workaround was an explicit intersection annotation on the capture type. All capture forms — bare field, bare local, expression capture, and object literal fields — now preserve the narrowed constraint without annotation.

## Don't box machine words smaller than 64 bits

On 64-bit platforms, Bool, U8, I8, U16, I16, U32, I32, and F32 are no longer heap-allocated when they appear in union types. On Windows (LLP64), ILong and ULong are also covered since `long` is 32-bit on that platform.

This eliminates a heap allocation every time one of these types appears in a union like `(U32 | None)` or `Any`, reducing GC pressure. Unboxing, match discrimination, and identity comparison no longer dereference a heap object for these types.

## Fix compiler crash on array literals passed by name or to a call on a literal

The compiler crashed when an array literal appeared as an argument in certain positions: as an operand to a numeric literal (`1 + [as U8: 2]`), as a named argument to a callable object (`f(where x = [as U8: 1])`), or through update sugar when `update` is a field holding a callable object. These now compile or produce a normal error message.

Passing an array literal to an object with no `apply` method, or to a tuple, reported the error twice. Each now reports one error.

## Fix PropertyRunner double completion on assert-and-error

In rare cases where a property test failed an assertion and raised an error, the runner sent two completion notifications instead of one, causing the test harness to fall out of sync. The completion notification is now sent exactly once.

## Fix stack overflow in itertools Iter methods

Several methods on `Iter` in the `itertools` package used unbounded recursion internally and could crash on larger iterators. This has been fixed.

## Fix compiler crash on `is` with a constructor call

Comparing a freshly constructed object with `is` or `isnt` is an error because the comparison is always false. Two shapes of constructor call crashed the compiler instead of reporting that error: a constructor with its own type parameters, as in `Foo[String].create[U8]("x", U8(1)) is y`, and a constructor called on a value, as in `x.create() is y`. Both now report the error.

A primitive's constructor returns the one instance, so comparing its result with `is` is allowed. That comparison crashed when the constructor had type parameters or was called on a value, and was wrongly rejected when the primitive was named through a type alias. All three now compile.

## Fix type parameter defaults that refer to an earlier type parameter

A type parameter default that named an earlier type parameter in the same list was not substituted at the use site. The raw reference survived into later passes, producing a spurious "type argument is outside its constraint" error or, when the type was used to construct an object, a compiler crash at code generation.

```pony
class Iter[A, I: Iterator[A] ref = Iterator[A] ref] is Iterator[A]
```

`Iter[U8]([0].values())` now compiles. The default `Iterator[A] ref` is reified to `Iterator[U8] ref` when `A` is `U8`. Default type arguments are now reified in all positions: type annotations, aliases, provides lists, partially explicit type argument lists, and calls.

## Fix compiler crash on a lambda type used as a type parameter default

A lambda type serving as a type parameter default captured that parameter, producing an unbound type parameter reference that reached code generation and crashed the compiler.

```pony
primitive Bar
  fun foo[A: Any val = {(U8): U8} val](a: A) => None
```

`Bar.foo(f)` now compiles. The generated interface for the lambda type no longer includes the defaulted parameter or its later siblings as type parameters, unless the lambda type's body names them.

## Fix compiler crash on a type parameter default naming itself or a later one

A type parameter default that named itself or a later type parameter in the same list made the compiler abort at code generation, or produced a misleading error about an internal name the user could not see from the use site.

```pony
class Foo[A: Any val = B, B: Any val = U8]

actor Main
  new create(env: Env) =>
    let x: Foo = Foo
```

The compiler now reports "not enough type arguments" with a continuation naming the type parameter the default refers to. A definition with such a default that is only ever used with written type arguments keeps compiling.

A type annotation that uses the default at a site that is never reached also becomes an error. `class Foo[A: Any val = A]` used only as `fun unused(x: Foo)` compiled before because the default was filled at the annotation and code generation never processed the unreached one. That program denotes a type with no meaning and is now rejected.

## Fix the capability of a partially applied constructor

Partially applying a zero-argument constructor produced an object that could not be called. The generated `apply` had a `ref` receiver, but the object itself was `val`, so calling it failed with "receiver type is not a subtype of target type."

```pony
class Bar
  new create() => None

actor Main
  new create(env: Env) =>
    let mk = Bar~create()
    let b = mk()  // failed: {(): Bar ref^} val is not a subtype of {(): Bar ref^} ref^
```

The same problem prevented annotating the partial as `{(): Bar ref^} val` or passing it to a behavior, even when all captured arguments were sendable.

The generated `apply` now gets a `box` receiver when no captured argument prevents it, so `val` objects can be called and sent across actors.

## Fix compiler crash on generic calls in runtime_override_defaults

Calling a primitive's function with type arguments inside `runtime_override_defaults` crashed the compiler with an assertion failure instead of compiling the program.

```pony
primitive P
  fun @get[B: Any val](b: B): U32 => 4

actor Main
  new create(env: Env) => None
  fun @runtime_override_defaults(rto: RuntimeOptions) =>
    rto.ponymaxthreads = P.get[U8](U8(1))
```

The same crash occurred with constructor calls that supply type arguments, such as `P.create[U8](U8(1)).get()`. Generic calls on primitives in `runtime_override_defaults` now compile as expected.

## Fix partial application of generic constructors

Partial application of a constructor on a generic type dropped the receiver's type arguments:

```pony
class Foo[A: Any val]
  new create(a: A) => None

actor Main
  new create(env: Env) =>
    let mk = Foo[U8]~create(1)
```

```
main.pony:6:28: not enough type arguments
```

Using a type alias or type parameter as the receiver crashed the compiler instead of compiling or reporting an error.

All three forms now work: `Foo[U8]~create(1)` compiles, `Bar~create(1)` through an alias compiles, and `A~create()` on a type parameter compiles when `A` is constrained to a type with that constructor.

## Fix type parameters constrained by other type parameters

When one type parameter was constrained by another, the compiler rejected code that treated the constrained parameter as a subtype of its constraint:

```pony
class A[X, Y: X]
  fun foo(y: Y) =>
    let x: X = consume y
```

The constraint `Y: X` declares that `Y` is a subtype of `X`, and the compiler now accepts this. Transitive chains also work: `Z: Y` and `Y: X` together imply `Z` is a subtype of `X`.

## Add min parameter to PonyCheck set and map generators

`Generators.set_of`, `Generators.set_is_of`, `Generators.map_of`, and `Generators.map_is_of` now accept `min` and `max` parameters, matching the existing convention in `Generators.seq_of`. The defaults are `min = 0` and `max = 100`.

To generate non-empty collections, pass `min = 1`:

```pony
let non_empty_set_gen =
  Generators.set_of[U8](Generators.u8() where min = 1)

let non_empty_map_gen =
  Generators.map_of[String, I64](
    Generators.zip2[String, I64](
      Generators.ascii_printable(1, 10),
      Generators.i64())
    where min = 1)
```

The `min` value is the minimum number of insertion attempts, not a guaranteed minimum collection size. Duplicate keys or values can reduce the final size below `min`.

## Change PonyCheck set and map generator parameter order from (gen, max) to (gen, min, max)

The parameter order for `Generators.set_of`, `Generators.set_is_of`, `Generators.map_of`, and `Generators.map_is_of` changed from `(gen, max)` to `(gen, min, max)` to match `Generators.seq_of`. Callers that passed `max` positionally need to switch to a named argument:

```pony
// Before
Generators.set_of[U8](Generators.u8(), 50)

// After
Generators.set_of[U8](Generators.u8() where max = 50)
```

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

