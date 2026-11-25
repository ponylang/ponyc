// Abstract generic methods are inherited through a chain of traits. The last
// trait and the class name their type parameter A, the same as the methods'.
// The class implements the methods under other type parameter names.
use @pony_exitcode[None](code: I32)

trait T1[X: Stringable val]
  fun f[A: X](a: A): X
  fun h[A: Stringable val, B: Array[A] val](b: B): USize

trait T2[Y: Stringable val] is T1[Y]

trait T3[A: Stringable val] is T2[A]

class C[A: Stringable val] is T3[A]
  fun f[B: A](b: B): A => b
  fun h[Q: Stringable val, R: Array[Q] val](r: R): USize => r.size()

actor Main
  new create(env: Env) =>
    let t: T1[String] = C[String]
    if t.f[String]("x") != "x" then @pony_exitcode(1) end
    if t.h[U8, Array[U8] val]([as U8: 1; 2]) != 2 then @pony_exitcode(2) end
    let t3: T3[String] = C[String]
    if t3.f[String]("y") != "y" then @pony_exitcode(3) end
