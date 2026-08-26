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

