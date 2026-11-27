// Each of c0, c1 and c2 captures this and returns a literal that wraps its
// name. Chaining c0, c1, c0 from C must print c0(c1(c0(p))).
trait P
  fun name(): String => "p"
  fun val c0(): P val =>
    let me = this
    object val is P
      fun name(): String => "c0(" + me.name() + ")"
    end
  fun val c1(): P val =>
    let me = this
    object val is P
      fun name(): String => "c1(" + me.name() + ")"
    end
  fun val c2(): P val =>
    let me = this
    object val is P
      fun name(): String => "c2(" + me.name() + ")"
    end
class val C is P
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.c0().c1().c0().name())
