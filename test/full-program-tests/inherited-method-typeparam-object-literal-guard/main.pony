// An object literal adds a specialization to an inherited generic method
// group. Its guard tests the object literal's own `R`, copied from `make`'s
// type parameter, not the method's `X`, so it doesn't replace the trait's
// specialization for `X`.
use @pony_exitcode[None](code: I32)

trait T
  fun f[X: Any val](x: X): U8 => 0
  fun f[X: Any val](x: X): U8 iftype X <: U32 val => 1

primitive Maker
  fun make[R: Any val](): T =>
    object ref is T
      fun f[X: Any val](x: X): U8 iftype R <: U32 val => 9
    end

actor Main
  new create(env: Env) =>
    if Maker.make[String]().f[U32](1) != 1 then @pony_exitcode(1) end
    if Maker.make[U32]().f[String]("a") != 9 then @pony_exitcode(2) end
