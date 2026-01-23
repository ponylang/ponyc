## Add Pony Language Server to the Ponyc distribution

We've moved the Pony Language Server that previously was distributed as it's own project to the standard ponyc distribution. This means that the language server will track changes in ponyc. Going forward, the language server that ships with a given ponyc will be fully up-to-date with the compiler and most importantly, it's version of the standard library.

## Handle Bool with exhaustive match

Previously, this function would fail to compile, as true and false were not seen as exhaustive, resulting in the match block exiting and the function attempting to return None.

```pony
  fun fourty_two(err: Bool): USize =>
    match err
    | true => return 50
    | false => return 42
    end
```

We have modified the compiler to recognize that if we have clauses for both true and false, that the match is exhaustive.

Consequently, you can expect the following to now fail to compile with unreachable code:

```pony
  fun forty_three(err: Bool): USize =>
    match err
    | true => return 50
    | false => return 43
    else
      return 44 // Unreachable
    end
```

## Fix ponyc crash when partially applying constructors

We've fixed a compilation assertion error that would result in a compiler crash when utilizing partial constructors.

Previously, this would fail to compile:

```pony
class A
  let n: USize

  new create(n': USize) =>
    n = n'

actor Main
  new create(env: Env) =>
    let ctor = A~create(2)
    let a: A = ctor()
```

## Add support for complex type formatting in LSP hover

The Pony Language Server now properly formats complex types in hover information. Previously, hovering over variables with union types, tuple types, intersection types, or arrow types would display internal token names like `TK_UNIONTYPE` instead of the actual type signature.

Now, the language server correctly displays formatted types such as:
- Union types: `(String | U32 | None)`
- Tuple types: `(String, U32, Bool)`
- Intersection types: `(ReadSeq[U8] & Hashable)`
- Arrow types: function signatures

