// Q[B2] is Foo[Array[U8] val, B2], and Foo.f tests A <: Array[B]. The result
// depends on Q's own parameter: Q[U8] prints then and Q[String] prints else.
trait Foo[A: Any val, B: Any val]
  fun f(): String =>
    iftype A <: Array[B] val then "then" else "else" end

class Q[B2: Any val] is Foo[Array[U8] val, B2]
  new create() => None

actor Main
  new create(env: Env) =>
    env.out.print(Q[U8].f())
    env.out.print(Q[String].f())
