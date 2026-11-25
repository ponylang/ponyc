// Types override one iftype specialization of a trait's generic method group,
// naming the method's type parameter differently from the trait. Each
// override must replace the trait's specialization with the same guard and
// leave the others in place.
use @pony_exitcode[None](code: I32)

trait T
  fun f[X: Any val](x: X): U8 => 0
  fun f[X: Any val](x: X): U8 iftype X <: U32 val => 1
  fun f[X: Any val](x: X): U8 iftype X <: (U32 val | U16 val) => 2

class C is T
  fun f[Y: Any val](x: Y): U8 iftype Y <: (U32 val | U16 val) => 22

interface val Scored
  fun score(): U64

primitive S
  fun score(): U64 => 5

trait Group
  fun check[A: Any val](arg: A): U64 => 10
  fun check[A: Any val](arg: A): U64 iftype A <: Scored => 20

primitive Combined is Group
  fun check[B: Any val](arg: B): U64 iftype B <: Scored => 30

actor Main
  new create(env: Env) =>
    if C.f[U32](1) != 1 then @pony_exitcode(1) end
    if C.f[U16](1) != 22 then @pony_exitcode(2) end
    if C.f[None](None) != 0 then @pony_exitcode(3) end
    if Combined.check[S](S) != 30 then @pony_exitcode(4) end
    if Combined.check[U8](1) != 10 then @pony_exitcode(5) end
