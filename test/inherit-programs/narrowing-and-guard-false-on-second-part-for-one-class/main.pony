// T's describe() has a specialization guarded by A <: Stringable val and
// B <: Stringable val. C1 fills B with MyVal, which is not Stringable, so the
// guard fails on its second part and the guarded body calling b.string() is
// no error in C1's copy. Expects "opaque" then "7 8".
class val MyVal
  new val create() => None

trait T[A: Any val, B: Any val]
  fun describe(a: A, b: B): String => "opaque"
  fun describe(a: A, b: B): String iftype A <: Stringable val and B <: Stringable val => a.string() + " " + b.string()

class C1 is T[U32, MyVal]
class C2 is T[U32, U32]

actor Main
  new create(env: Env) =>
    env.out.print(C1.describe(7, MyVal))
    env.out.print(C2.describe(7, 8))
