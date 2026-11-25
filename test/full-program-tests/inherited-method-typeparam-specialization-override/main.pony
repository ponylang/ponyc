// Classes override a trait's generic method group: the default and an iftype
// specialization. `C` uses the trait's type parameter name; `D` uses a
// different one, and has its own type parameter with the trait's name.
use @pony_exitcode[None](code: I32)

trait T
  fun f[X: Any val](x: X): I32 => 0
  fun f[X: Any val](x: X): I32 iftype X <: U32 val => 1

class C is T
  fun f[X: Any val](x: X): I32 => 10
  fun f[X: Any val](x: X): I32 iftype X <: U32 val => 11

class D[X: Any val] is T
  fun f[Y: Any val](y: Y): I32 => 20
  fun f[Y: Any val](y: Y): I32 iftype Y <: U32 val => 21

actor Main
  new create(env: Env) =>
    if (C.f[U32](1) != 11) or (C.f[None](None) != 10) then
      @pony_exitcode(1)
    end
    if (D[None].f[U32](1) != 21) or (D[None].f[None](None) != 20) then
      @pony_exitcode(2)
    end
