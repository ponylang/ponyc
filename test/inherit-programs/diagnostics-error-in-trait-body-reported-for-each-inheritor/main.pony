// T's f() returns a String where U8 is declared, and P1 and P2 both inherit
// it. Expects "function body isn't the result type" three times: once for T
// and once for each primitive, with a note naming the inheriting type (#6724).
trait T
  fun f(): U8 => "x"

primitive P1 is T
primitive P2 is T

actor Main
  new create(env: Env) => None
