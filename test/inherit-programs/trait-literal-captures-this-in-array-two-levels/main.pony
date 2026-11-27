// T0.fa and T1.fa each put this in an array and return an object literal that
// reads it. Chaining C.fa().fa() builds a literal inside a literal's class.
// Expects aat0.
trait T0
  fun name(): String => "t0"
  fun val fa(): T1 val =>
    let me = recover val [this] end
    object val is T1
      fun name(): String => "a" + try me(0)?.name() else "" end
    end
trait T1
  fun name(): String => "t1"
  fun val fa(): T2 val =>
    let me = recover val [this] end
    object val is T2
      fun name(): String => "a" + try me(0)?.name() else "" end
    end
trait T2
  fun name(): String => "t2"
class val C is T0
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.fa().fa().name())
