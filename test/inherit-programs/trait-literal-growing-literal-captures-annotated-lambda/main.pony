// T[A].m() returns a T[Array[A] val] literal that captures a lambda stored
// in a local with a written type. Calling n() on C.m() and on three levels of
// m() must print "1 1": the local must be assigned in every inherited copy
// and the captured lambda's type must match the field at every depth.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val =>
    let f: {(): USize} val = {(): USize => Array[A].size() + 1}
    object val is T[Array[A] val]
      fun n(): String => f().string()
    end
class val C is T[U8]
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(C.m().n() + " " + C.m().m().m().n())
