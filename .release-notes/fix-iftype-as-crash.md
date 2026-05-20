## Fix compiler crash when combining `iftype` and `as`

Previously, applying `as` to the result of an `iftype` expression that contained a method call on a narrowed type parameter would crash the compiler with an internal assertion failure (ponylang/ponyc#2042):

```pony
class LitString
interface AST
interface HasDocs
  fun val docs(): (LitString | None)

actor Main
  new create(env: Env) => None
  fun foo[A: AST val](node: A) =>
    try
      iftype A <: HasDocs
      then node.docs()
      end as LitString
    end
```

The compiler now compiles this code correctly.
