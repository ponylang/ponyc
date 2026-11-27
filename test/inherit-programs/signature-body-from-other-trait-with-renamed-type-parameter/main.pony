// T1 declares f[A] with no body and T2 gives f[B] a body; C provides both.
// The type parameters differ only in name, so the test expects C to take
// T2's body and print 3 (#6733).
trait T1
  fun f[A: Stringable val](x: A): String

trait T2
  fun f[B: Stringable val](x: B): String => x.string()

class C is (T1 & T2)

actor Main
  new create(env: Env) =>
    env.out.print(C.f[U8](3))
