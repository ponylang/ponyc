// D overrides run with a lambda type that spells out the val capabilities T
// leaves implicit. T.go's untyped lambda must type in both C and D, printing
// C:3 and D:4.
trait T
  fun run(f: {(U8): String} val): String
  fun go(): String => run({(x) => x.string()})
class C is T
  new create() => None
  fun run(f: {(U8): String} val): String => "C:" + f(3)
class D is T
  new create() => None
  fun run(f: {(U8 val): String val} val): String => "D:" + f(4)
actor Main
  new create(env: Env) =>
    env.out.print(C.go())
    env.out.print(D.go())
