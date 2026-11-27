// T.m() returns a T literal that captures a lambda. Calling n() on C.m() and
// on three levels of m() must print "1 1": the local holding the lambda must
// be assigned in every inherited copy of m().
trait T
  fun n(): String => "n"
  fun m(): T val =>
    let f = {(): USize => 1}
    object val is T
      fun n(): String => f().string()
    end
class val C is T
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(C.m().n() + " " + C.m().m().m().n())
