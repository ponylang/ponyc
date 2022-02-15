## Fix soundness bug introduced

We previously switched our underlying type system model. In the process, a
soundness hole was introduced where users could assign values to variables with
an ephemeral capability. This could lead to "very bad things".

The hole was introduced in May of 2021 by [PR #3643](https://github.com/ponylang/ponyc/pull/3643). We've closed the hole so that code such as the following will again be rejected by the compiler.

```pony
let map: Map[String, Array[String]] trn^ = recover trn Map[String, Array[String]] end
```

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

```ponyc
type Blocksize is (U8, U32)
class Block[T: Blocksize]
```

We've added an additional check to the compiler in a later pass to report an error on the "smuggled" tuple type as a constraint.

