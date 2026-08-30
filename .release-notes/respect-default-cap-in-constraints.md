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
