// A trait's default argument is a lambda over the trait's type parameter A,
// and nothing inherits the trait. The program must compile; expects ok.
trait T[A: Stringable val]
  fun show(a: A, f: {(A): String} val = {(x: A): String => "x:" + x.string()}): String =>
    f(a)
actor Main
  new create(env: Env) => env.out.print("ok")
