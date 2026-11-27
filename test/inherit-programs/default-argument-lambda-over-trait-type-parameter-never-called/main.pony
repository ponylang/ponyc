// T.show's default is a lambda over T's type parameter A. C provides T[U8]
// and nothing calls show; the program must still compile and print ok.
trait T[A: Stringable val]
  fun show(a: A, f: {(A): String} val = {(x: A): String => "x:" + x.string()}): String =>
    f(a)
class C is T[U8]
actor Main
  new create(env: Env) => env.out.print("ok")
