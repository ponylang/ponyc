// As the non-generic version with a type parameter on P: each of c0, c1 and
// c2 captures this and wraps its name. Chaining c0, c1, c0 from C must print
// c0(c1(c0(p))).
trait P[A: Any val]
  fun name(): String => "p"
  fun val c0(): P[A] val =>
    let me = this
    object val is P[A]
      fun name(): String => "c0(" + me.name() + ")"
    end
  fun val c1(): P[A] val =>
    let me = this
    object val is P[A]
      fun name(): String => "c1(" + me.name() + ")"
    end
  fun val c2(): P[A] val =>
    let me = this
    object val is P[A]
      fun name(): String => "c2(" + me.name() + ")"
    end
class val C is P[U8]
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.c0().c1().c0().name())
