// m captures a tuple holding this. C's literal names C and the literal made
// from it names the first literal, printing "inner of C" then
// "inner of inner of C".
trait T
  fun name(): String => "T"
  fun val m(): T val =>
    let pair = (this, U8(1))
    object val is T
      fun name(): String => "inner of " + pair._1.name()
    end
primitive C is T
  fun name(): String => "C"
actor Main
  new create(env: Env) => env.out.print(C.m().name()); env.out.print(C.m().m().name())
