// T.run returns a lambda that calls Check, whose default argument is __loc.
// Expects line 8, where the lambda is written in T, for the lambdas from C
// and D, and those two lambdas to be the same object.
primitive Check
  fun apply(b: Bool, loc: SourceLoc = __loc): String =>
    if b then "ok" else "fail@" + loc.line().string() end
trait T
  fun run(): {(): String} val => {(): String => Check(false)}
  fun run2(): String => Check(true)
class val C is T
  new val create() => None
class val D is T
  new val create() => None
actor Main
  new create(env: Env) =>
    let t: T val = C
    env.out.print(C.run()() + " " + D.run()() + " " + C.run2() + " " + (C.run() is D.run()).string())
