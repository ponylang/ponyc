// T[A].m() returns an N literal that captures a lambda using A. C is T[U8],
// so C.m().n() must print "1": the lambda and the literal must be typed with
// the A that C provides.
interface val N
  fun n(): String
trait T[A: Any val]
  fun m(): N =>
    let f = {(): USize => Array[A].size() + 1}
    object val is N
      fun n(): String => f().string()
    end
class val C is T[U8]
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(C.m().n())
