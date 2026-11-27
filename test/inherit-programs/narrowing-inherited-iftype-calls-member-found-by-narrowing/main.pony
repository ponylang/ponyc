// Foo.f calls x.size() only when A <: Array[B]. Q[B2] is Foo[Array[U8] val,
// B2]: Q[U8] takes the then branch and prints 2, Q[String] prints else.
trait Foo[A: Any val, B: Any val]
  fun f(x: A): String =>
    iftype A <: Array[B] val then x.size().string() else "else" end
class Q[B2: Any val] is Foo[Array[U8] val, B2]
  new create() => None
class R[B2: Any val] is Foo[String, B2]
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(Q[U8].f([1;2]))
    env.out.print(Q[String].f([1;2]))
