// T.m() builds an object literal whose k() returns v.string() where U8 is
// required. C is declared before T. The program must fail with "function
// body isn't the result type" and a note naming m() inherited by C from T.
class C is T
trait T
  fun m(): Any val =>
    let v: U8 = 1
    object val fun k(): U8 => v.string() end
actor Main
  new create(env: Env) => C.m()
