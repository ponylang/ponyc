// A trait's default argument is a lambda that uses the trait's type parameter
// A, and the program calls show(1) through a T[U8] reference to C. The test
// expects "x:1", and C's own copy of the method must also compile.
trait T[A: Stringable val]
  fun show(a: A, f: {(A): String} val = {(x: A): String => "x:" + x.string()}): String =>
    f(a)

class C is T[U8]

actor Main
  new create(env: Env) =>
    let t: T[U8] = C
    env.out.print(t.show(1))
