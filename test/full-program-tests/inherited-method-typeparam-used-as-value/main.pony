// A trait's generic default method uses its own type parameter `R` as a
// value, and the class that inherits it has its own `R`.
use @pony_exitcode[None](code: I32)

interface val Mk
  new val create()
  fun v(): U8

primitive Seven is Mk
  fun v(): U8 => 7

primitive Eight is Mk
  fun v(): U8 => 8

trait T
  fun make[R: Mk val](): U8 => R.create().v()

class C[R: Mk val] is T

actor Main
  new create(env: Env) =>
    if C[Eight].make[Seven]() != 7 then @pony_exitcode(1) end
