// A trait's generic default methods narrow their own type parameter with a
// condition that mentions it, in the body and as a specialization guard.
use @pony_exitcode[None](code: I32)

trait Body
  fun pick[B: Any val](b: B): U8 =>
    iftype B <: Comparable[B] val then 1 else 0 end

trait Guard
  fun pick[B: Any val](b: B): U8 => 0
  fun pick[B: Any val](b: B): U8 iftype B <: Comparable[B] val => 1

primitive Plain

primitive P is Body
class C is Guard

actor Main
  new create(env: Env) =>
    if P.pick[U16](1) != 1 then @pony_exitcode(1) end
    if P.pick[Plain](Plain) != 0 then @pony_exitcode(2) end
    if C.pick[U16](1) != 1 then @pony_exitcode(3) end
    if C.pick[Plain](Plain) != 0 then @pony_exitcode(4) end
