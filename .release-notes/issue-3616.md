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
