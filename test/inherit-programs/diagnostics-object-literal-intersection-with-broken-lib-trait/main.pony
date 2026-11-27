// An object literal in Main provides (l.Foo & M), and l.Foo's default has a
// type error. The test expects exactly that one error: the literal's check of
// l.Foo ends the pass before Main's later type error, the same as when the
// literal provides l.Foo alone.
use l = "lib"
trait M
actor Main
  new create(env: Env) =>
    let o = object is (l.Foo & M) end
    let x: U8 = "main"
