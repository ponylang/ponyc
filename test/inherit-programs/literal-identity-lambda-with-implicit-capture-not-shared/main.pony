// The lambda in g captures the parameter x without listing it, so the class
// each of C's and D's copies of g makes has a field for x, and the two copies
// can't share one class. The test expects each call to add its own x,
// printing "11 21".
trait T
  fun app(f: {(U64): U64} val): U64 => f(1)
  fun g(x: U64): U64 => app({(y) => x + y})
class val C is T
  new val create() => None
class val D is T
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.g(10).string() + " " + D.g(20).string())
