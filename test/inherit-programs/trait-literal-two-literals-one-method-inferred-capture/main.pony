// m returns two literals; the second captures s = n(), with an inferred
// type, and provides T[Array[A] val]. Calling m on C and on the second
// literal must print nnnn.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): (T[A] val, T[Array[A] val] val) =>
    let s = n()
    (object val is T[A] end, object val is T[Array[A] val] fun n(): String => s end)
class val C is T[U8]
  new val create() => None
actor Main
  new create(env: Env) =>
    (let a, let b) = C.m()
    (let c, let d) = b.m()
    env.out.print(a.n() + b.n() + c.n() + d.n())
