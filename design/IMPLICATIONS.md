# Implications

These follow from the principles in PRINCIPLES.md but aren't principles themselves.

## Object literals

### An object literal written once is one type

An object literal written once in a trait's code is one type in every type that provides the trait. The same holds for a lambda, which is written as an object literal.

This follows from two principles in PRINCIPLES.md. By the Traits principle, a trait's code means the same thing in every type that provides the trait as it does in the trait. By the Object literals principle, an object literal can be swapped for the named class it stands for, and a named class declared once is one type.

`is` checks identity: whether two values are the same object. It doesn't compare types. An object literal that captures nothing has no fields, so it has one object. For it, being one type means `is` is true wherever the code runs. An object literal that captures values makes a new object each time it is evaluated, so `is` is false between two calls even within one type. For it, being one type doesn't show in what the program does.

This prints "true". Both calls evaluate the object literal written once in `T`, so they give the same object:

```pony
trait T
  fun m(): T val => object val is T end

class C1 is T
  new create() => None

class C2 is T
  new create() => None

actor Main
  new create(env: Env) =>
    env.out.print((C1.m() is C2.m()).string())
```

A lambda works the same way. This prints "true":

```pony
trait L
  fun app(f: {(U64): U64} val): {(U64): U64} val => f
  fun g(): {(U64): U64} val => app({(x) => x + 1})

class D1 is L
  new create() => None

class D2 is L
  new create() => None

actor Main
  new create(env: Env) =>
    env.out.print((D1.g() is D2.g()).string())
```
