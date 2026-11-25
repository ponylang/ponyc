// Classes override a trait's specialization whose guard narrows the type
// parameter's capability. One names the type parameter as the trait does and
// one names it differently.
use @pony_exitcode[None](code: I32)

trait T
  fun f[X: Stringable](x: X): U8 => 0
  fun f[X: Stringable](x: X): U8 iftype X <: U32 val => 1

class Same is T
  fun f[X: Stringable](x: X): U8 iftype X <: U32 val => 21

class Renamed is T
  fun f[Y: Stringable](x: Y): U8 iftype Y <: U32 val => 22

actor Main
  new create(env: Env) =>
    if Same.f[U32](1) != 21 then @pony_exitcode(1) end
    if Same.f[String]("a") != 0 then @pony_exitcode(2) end
    if Renamed.f[U32](1) != 22 then @pony_exitcode(3) end
    if Renamed.f[String]("a") != 0 then @pony_exitcode(4) end
