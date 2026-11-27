// A trait's default argument is a lambda that uses the trait's type parameter
// A, and the program calls C.show(1) without that argument. The test expects
// "x:1": A in the lambda resolves to the trait's parameter, bound to U8.
trait T[A: Stringable val]
  fun show(a: A, f: {(A): String} val = {(x: A): String => "x:" + x.string()}): String =>
    f(a)

class C is T[U8]

actor Main
  new create(env: Env) =>
    env.out.print(C.show(1))
