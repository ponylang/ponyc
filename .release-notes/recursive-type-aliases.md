## Allow finite recursive type aliases

Type aliases can now reference themselves, as long as the resulting type has a finite layout. The compiler used to reject every self-referential alias, including ones that would have been perfectly fine to construct — JSON-like data, parse trees, and other tree-shaped patterns couldn't be expressed as aliases.

```pony
use "collections"

// Legal: JSON-like recursive structure.
type JsonValue is
  ( String
  | F64
  | Bool
  | None
  | Array[JsonValue]
  | Map[String, JsonValue])

// Legal: tree built from a generic carrier.
type Tree is (None | Array[Tree])
```

The aliases declare type *shape*. As with any Pony type, sending a value of one of these aliases across actors needs a sendable capability — the JSON example used for messaging would typically be `JsonValue val`, with the inner Array and Map constructed inside `recover val ... end` blocks.

Recursion is allowed when the cycle threads through some generic class's, actor's, or other nominal type's type-argument position (like `Array[T]` or `Map[K, V]`) and there's a union somewhere reachable from the cycle with at least one member that doesn't loop back. Without both, the recursion is infinite. The most common illegal shapes:

```pony
// Illegal: bare self-reference. The recursive arm has no
// constructor wrapping it, so every value is just None.
type A is (A | None)

// Illegal: recursion through a tuple element. Pony tuples are
// inline value types, so each IntList value would need to inline
// another IntList transitively. No finite layout exists.
type IntList is (None | (U32, IntList))

// Illegal: the recursion threads through Array's type argument,
// but no union with a non-recursive alternative is reachable, so
// there's no base case to stop at.
type A is Array[A]
```

When you write something illegal, the compiler reports `type alias 'X' can't be infinitely recursive` along with a note suggesting the fix. Either thread the recursion through the type argument of a generic class (or other nominal type) like `Array[X]`, or add a non-recursive alternative in a union (`(None | <body>)`).
