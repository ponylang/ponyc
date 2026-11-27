// A generic primitive P[A] inherits show(), whose default argument is a lambda
// over A. P[U8].show(1) must use that default; expects x:1.
trait T[A: Stringable val]
  fun show(a: A, f: {(A): String} val = {(x: A): String => "x:" + x.string()}): String =>
    f(a)

primitive P[A: Stringable val] is T[A]

actor Main
  new create(env: Env) =>
    env.out.print(P[U8].show(1))
