// An iftype in an object literal in T's f() narrows (A, A), which names A
// twice. Expects "can't reuse name 'A'" twice, with the "can't reuse name
// 'g'" error ponyc also reports between them.
trait T[A]
  fun f(): String =>
    let o = object
      fun g(): String =>
        iftype (A, A) <: (Stringable, Stringable) then "yes" else "no" end
    end
    o.g()

class C is T[U8]

actor Main
  new create(env: Env) =>
    env.out.print(C.f())
