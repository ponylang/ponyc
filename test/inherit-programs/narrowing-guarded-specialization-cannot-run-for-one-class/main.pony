// T's describe() has a specialization guarded by iftype A <: Stringable val.
// C1 fills A with MyVal, which is not Stringable, and C2 with U32. Expects
// "opaque" then "7": C1 uses the default body, and the guarded one calling
// a.string() is not an error in C1's copy.
class val MyVal
  new val create() => None

trait T[A: Any val]
  fun describe(a: A): String => "opaque"
  fun describe(a: A): String iftype A <: Stringable val => a.string()

class C1 is T[MyVal]
class C2 is T[U32]

actor Main
  new create(env: Env) =>
    env.out.print(C1.describe(MyVal))
    env.out.print(C2.describe(7))
