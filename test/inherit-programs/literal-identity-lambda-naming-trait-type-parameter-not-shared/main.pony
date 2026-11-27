// The lambda in g names T's type parameter A, which C gives as U8 and D as
// U64, so C's and D's copies of g each make their own class even though the
// lambda is written the same in both. The test expects 200 + 200 to wrap in
// U8 for C and not in U64 for D, printing "144 400".
trait T[A: (Stringable val & Real[A] val)]
  fun app(f: {(U8): String} val): String => f(200)
  fun g(): String => app({(y) => (A.from[U8](y) + A.from[U8](y)).string()})
class val C is T[U8]
  new val create() => None
class val D is T[U64]
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.g() + " " + D.g())
