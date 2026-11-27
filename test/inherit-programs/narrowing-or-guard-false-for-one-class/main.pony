// T's describe() is specialized for A <: U8 val or A <: U16 val. C1 fills A
// with MyVal, which is neither, so the guarded body is no error in C1's copy.
// Expects "opaque" then "7".
class val MyVal
  new val create() => None

trait T[A: Any val]
  fun describe(a: A): String => "opaque"
  fun describe(a: A): String iftype A <: U8 val or A <: U16 val => a.string()

class C1 is T[MyVal]
class C2 is T[U16]

actor Main
  new create(env: Env) =>
    env.out.print(C1.describe(MyVal))
    env.out.print(C2.describe(7))
