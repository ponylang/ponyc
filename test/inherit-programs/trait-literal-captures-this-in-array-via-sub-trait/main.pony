// C is S, and S overrides name() from T0. T0.fa captures this in an array and
// returns a literal that calls name() on it, so the call must reach S's name().
// Expects as.
trait T0
  fun name(): String => "t0"
  fun val fa(): T2 val =>
    let me = recover val [this] end
    object val is T2
      fun name(): String => "a" + try me(0)?.name() else "" end
    end
trait S is T0
  fun name(): String => "s"
trait T2
  fun name(): String => "t2"
class val C is S
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.fa().name())
