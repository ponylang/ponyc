primitive Foo
  fun apply() : (U32 | None) => 1

type Bar is (U32 | None)

actor Main
  let bar : Bar

  new create(env : Env) =>
      match \exhaustive\ Foo()
      | let result : U32 =>
          bar = result
      | None =>
          bar = 0
      end
