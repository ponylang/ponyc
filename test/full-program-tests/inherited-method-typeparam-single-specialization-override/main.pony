// A class overrides one iftype specialization of a trait's generic method
// group. The override must replace that specialization and leave the others.
use @pony_exitcode[None](code: I32)

trait T
  fun f[X: Any val](x: X): U8 => 0
  fun f[X: Any val](x: X): U8 iftype X <: U32 val => 1
  fun f[X: Any val](x: X): U8 iftype X <: U16 val => 2

class C is T
  fun f[X: Any val](x: X): U8 iftype X <: U16 val => 22

actor Main
  new create(env: Env) =>
    if C.f[U32](1) != 1 then @pony_exitcode(1) end
    if C.f[U16](1) != 22 then @pony_exitcode(2) end
    if C.f[None](None) != 0 then @pony_exitcode(3) end
