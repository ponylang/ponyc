// T[A].m() returns a T[Array[A] val] literal that captures another object
// literal using A. Calling n() on C.m() and on three levels of m() must print
// "w0 w0": the captured object's type must match the field at every depth.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val =>
    let s: String val = "w"
    let g = object val fun w(): String => s + Array[A].size().string() end
    object val is T[Array[A] val]
      fun n(): String => g.w()
    end
class val C is T[U8]
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(C.m().n() + " " + C.m().m().m().n())
