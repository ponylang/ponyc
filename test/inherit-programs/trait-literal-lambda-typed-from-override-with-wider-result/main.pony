// T.go passes an untyped lambda to run. C overrides run to take a lambda that
// returns Stringable; D keeps String. Each class must type the lambda from its
// own run, printing C:3 and D:4.
trait T
  fun run(f: {(U8): String} val): String
  fun go(): String => run({(x) => x.string()})
class C is T
  new create() => None
  fun run(f: {(U8): Stringable} val): String => "C:" + f(3).string()
class D is T
  new create() => None
  fun run(f: {(U8): String} val): String => "D:" + f(4)
actor Main
  new create(env: Env) =>
    env.out.print(C.go())
    env.out.print(D.go())
