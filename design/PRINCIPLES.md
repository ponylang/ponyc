#

## Declarations

### Order doesn't change meaning

The order things are written in doesn't change what a program means. That covers the order of declarations in a file or package, so a type can be used before it is declared. It also covers the order of types in a provides list or any other intersection: `(S1 & S2)` and `(S2 & S1)` are the same type. If reordering changes what a program does or whether it compiles, that's a bug.

The two object literals below differ only in the order they list `S1` and `S2`. Both print "other":

```pony
primitive Name
  fun apply[X](x: X): String =>
    iftype X <: U64 then "u64" else "other" end

trait T0
  fun get(): (U64 | None)
  fun label(): String => Name(get())

trait S1 is T0
  fun get(): U64

trait S2 is T0

trait M
  fun first(): T0 val => object val is (S1 & S2) fun get(): U64 => 1 end
  fun second(): T0 val => object val is (S2 & S1) fun get(): U64 => 1 end

class val C is M
  new val create() => None

actor Main
  new create(env: Env) =>
    env.out.print(C.first().label())
    env.out.print(C.second().label())
```

`label()` is written in `T0`, where `get()` returns `(U64 | None)`. That follows from the Traits principle below. The order `S1` and `S2` are listed in doesn't matter.

A class may be declared before or after the trait it provides, with the same result. Here `Bar` comes before `Foo`, and `Bar.name()` returns "foo". With `Foo` declared first, it returns "foo" too.

```pony
class Bar is Foo
  new create() => None

trait Foo
  fun name(): String => "foo"
```

## Subtyping and method lookup

### A call is typed by the receiver's static type

When you call a method on a value, the call has the signature that the method has on the value's static type. If a class narrows the return type of a trait's method, a call through the class gets the narrower type, and a call through the trait gets the trait's type.

```pony
trait Foo
  fun get(): (U64 | String) => "s"

class C is Foo
  new create() => None
  fun get(): U64 => 7

actor Main
  new create(env: Env) =>
    let c = C
    let f: Foo = c
    let a = c.get()  // U64
    let b = f.get()  // (U64 | String)
```

### An override may return a narrower type and accept wider ones

A method that implements a trait's method may return a narrower type than the trait declares, and may accept wider parameter types. The reverse is a compile error: a class whose `get()` returns `(U64 | String)` cannot provide a trait whose `get()` returns `U64`. This rule is what makes typing a call by the static type safe. A value used as a `Source` never returns something a `Source` caller can't handle.

This compiles:

```pony
trait Source
  fun get(): (U64 | String)
  fun put(x: U64): String

class Narrow is Source
  new create() => None
  fun get(): U64 => 7
  fun put(x: (U64 | String)): String => "wide"
```

### Which body runs is decided at runtime, but the types are not

A call runs the method body of the value's actual type. The call's type still comes from the static type. Calling `get()` on a `Source` that holds a `Narrow` runs `Narrow`'s `get`, and the result is typed `(U64 | String)`. A `match` recovers the `U64`. Using `Source` and `Narrow` from above, this prints "runs Narrow's get: 7":

```pony
actor Main
  new create(env: Env) =>
    let s: Source = Narrow
    match s.get()
    | let n: U64 => env.out.print("runs Narrow's get: " + n.string())
    | let t: String => env.out.print(t)
    end
```

## Privacy

### Private types are published through public types

A type is accessible outside its package if it is public or a public type alias of its package names it. You can call any public method on a value whose type is accessible. Private methods and fields stay private to their package, whatever type you reach them through.

`_H` is private to `lib`, but the public alias `PubH` names it, so `main` can call `name()` on it. This compiles and prints "lib._H":

```pony
// lib/lib.pony
primitive _H
  fun name(): String => "lib._H"

type PubH is _H

primitive MakeH
  fun apply(): PubH => _H
```

```pony
// main.pony
use l = "lib"

actor Main
  new create(env: Env) =>
    let x: l.PubH = l.MakeH()
    env.out.print(x.name())
```

Without the alias, with `MakeH` returning `_H`, `main` can hold the value but can't call `name()` through its type. That call is a compile error: "can't lookup fields or methods on private types from other modules". `main` can still use the value through a public interface that `_H` satisfies, and call `name()` there:

```pony
// lib/lib.pony
interface HasName
  fun name(): String

primitive _H
  fun name(): String => "lib._H"

primitive MakeH
  fun apply(): _H => _H
```

```pony
// main.pony
use l = "lib"

actor Main
  new create(env: Env) =>
    let h: l.HasName val = l.MakeH()
    env.out.print(h.name())   // prints "lib._H"
    // l.MakeH().name() is an error: _H has no public name
```

The standard library's `http_client` package uses this. It has `type Version is ((HTTP10 | HTTP11) & _Version)`, where `_Version` is a private interface. The alias publishes `_Version`'s methods, `string()` and `==`, on `Version`.

## Traits

### Inherited code means what it means where it's written

A trait's code means the same thing in every type that provides the trait as it does in the trait. Inside a default method, `this` has the trait's type. So by the principles above, calls on `this` are typed by the trait's declarations, even in a type that overrides them with narrower ones. Names in the trait's code resolve where the trait is written, and calls in the trait's code are the trait's package's calls, so privacy follows the Privacy principle as the trait's package sees it. At runtime, calls still run the implementing type's method bodies.

The test: if a trait's code compiles in the trait, it compiles in every type that provides the trait. Nothing an implementer does, such as a legal narrower override, can make it fail.

This compiles and prints "ok":

```pony
trait Foo
  fun mk(): (U8 | None)
  fun f(): String =>
    var v = mk()
    v = None
    "ok"

class C is Foo
  new create() => None
  fun mk(): U8 => 3

actor Main
  new create(env: Env) =>
    env.out.print(C.f())
```

In `f()`, `mk()` has type `(U8 | None)` as `Foo` declares it, so `v` is `(U8 | None)` and `v = None` is allowed, even though `C`'s `mk()` returns `U8`.

Names and privacy work the same way. This program has two files and prints "lib's Helper" then "lib's private _Secret":

```pony
// lib/lib.pony
primitive Helper
  fun name(): String => "lib's Helper"

primitive _Secret
  fun value(): String => "lib's private _Secret"

trait Greeter
  fun helper(): String => Helper.name()
  fun secret(): String => _Secret.value()
```

```pony
// main.pony
use l = "lib"

primitive Wrap
  fun name(): String => "main's Wrap"

class Bar[Helper: Wrap val] is l.Greeter
  new create() => None

actor Main
  new create(env: Env) =>
    let b = Bar[Wrap]
    env.out.print(b.helper())
    env.out.print(b.secret())
```

`Helper` in `Greeter`'s `helper()` is the primitive from `lib`, not `Bar`'s type parameter named `Helper`. `_Secret` is private to `lib`, and the call to it is `lib`'s code, so `Bar` can run it.

## Interfaces

- Interfaces should behave exactly like traits except that they use structual typing not nominal and that private methods are not allowed

## Object literals

- Object literals should be swappable with the named equivalent with no change in behavior or performance.
