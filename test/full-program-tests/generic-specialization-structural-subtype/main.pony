// Classes' generic method groups are subtypes of interfaces with the same
// method groups. The specializations are guarded by a union, by an interface,
// by a type that names another of the method's type parameters, and by a type
// that names the interface's type parameter. One class names its method's type
// parameters differently from the interface. The classes are assigned to the
// interfaces directly and through an intersection.
use @pony_exitcode[None](code: I32)

interface I
  fun f[X: Any val](x: X): U8 => 0
  fun f[X: Any val](x: X): U8 iftype X <: (U32 val | U16 val) => 1

interface J
  fun g(): U8

class D
  fun f[X: Any val](x: X): U8 => 10
  fun f[X: Any val](x: X): U8 iftype X <: (U32 val | U16 val) => 11
  fun g(): U8 => 3

interface K
  fun f[A: Any val, B: Any val](a: A, b: B): U8 => 0
  fun f[A: Any val, B: Any val](a: A, b: B): U8 iftype A <: ReadSeq[B] val =>
    1

class E
  fun f[A: Any val, B: Any val](a: A, b: B): U8 => 20
  fun f[A: Any val, B: Any val](a: A, b: B): U8 iftype A <: ReadSeq[B] val =>
    21
  fun g(): U8 => 4

interface L[X: Any val]
  fun f[A: Any val](a: A): U8 => 0
  fun f[A: Any val](a: A): U8 iftype A <: Array[X] val => 1

class F[X: Any val]
  fun f[A: Any val](a: A): U8 => 30
  fun f[A: Any val](a: A): U8 iftype A <: Array[X] val => 31

class G
  fun f[P: Any val, Q: Any val](a: P, b: Q): U8 => 40
  fun f[P: Any val, Q: Any val](a: P, b: Q): U8 iftype P <: ReadSeq[Q] val =>
    41
  fun g(): U8 => 5

interface M
  fun f[A: Any val](a: A): U8 => 0
  fun f[A: Any val](a: A): U8 iftype A <: Stringable val => 1

class H
  fun f[A: Any val](a: A): U8 => 50
  fun f[A: Any val](a: A): U8 iftype A <: Stringable val => 51

actor Main
  new create(env: Env) =>
    let i: I = D
    if i.f[U32](1) != 11 then @pony_exitcode(1) end
    if i.f[U8](1) != 10 then @pony_exitcode(2) end
    let dj: (D & J) = D
    let i2: I = dj
    if i2.f[U16](1) != 11 then @pony_exitcode(3) end
    let k: K = E
    let arr: Array[U8] val = recover val [1] end
    if k.f[Array[U8] val, U8](arr, 1) != 21 then @pony_exitcode(4) end
    if k.f[U8, U8](1, 1) != 20 then @pony_exitcode(5) end
    let ej: (E & J) = E
    let k2: K = ej
    if k2.f[Array[U8] val, U8](arr, 1) != 21 then @pony_exitcode(6) end
    let l: L[U8] = F[U8]
    if l.f[Array[U8] val](arr) != 31 then @pony_exitcode(7) end
    if l.f[U8](1) != 30 then @pony_exitcode(8) end
    let k3: K = G
    if k3.f[Array[U8] val, U8](arr, 1) != 41 then @pony_exitcode(9) end
    if k3.f[U8, U8](1, 1) != 40 then @pony_exitcode(10) end
    let gj: (G & J) = G
    let k4: K = gj
    if k4.f[Array[U8] val, U8](arr, 1) != 41 then @pony_exitcode(11) end
    let m: M = H
    if m.f[String]("s") != 51 then @pony_exitcode(12) end
    if m.f[Array[U8] val](arr) != 50 then @pony_exitcode(13) end
