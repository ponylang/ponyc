// An iftype in an object literal in T's f() narrows (A, String), and String
// is not a type parameter. Expects the error at String twice, with the
// "can't reuse name 'g'" error ponyc also reports between them.
trait T[A]
  fun f(): String =>
    let o = object
      fun g(): String =>
        iftype (A, String) <: (Stringable, Stringable) then "yes" else "no" end
    end
    o.g()

class C is T[U8]

actor Main
  new create(env: Env) =>
    env.out.print(C.f())
