// Types that inherit a generic method group with a specialization from a trait
// or interface are subtypes of an interface with the same method group: a
// class, the class through an intersection, a generic class whose type
// parameter has the same name as the method's, a primitive and an object
// literal.
use @pony_exitcode[None](code: I32)

interface I
  fun f[A: Any val](a: A): U8
  fun f[A: Any val](a: A): U8 iftype A <: U32 val

interface val IV
  fun f[A: Any val](a: A): U8
  fun f[A: Any val](a: A): U8 iftype A <: U32 val

interface J
  fun g(): U8

trait T
  fun f[A: Any val](a: A): U8 => 0
  fun f[A: Any val](a: A): U8 iftype A <: U32 val => 1

interface I2
  fun f[A: Any val](a: A): U8 => 10
  fun f[A: Any val](a: A): U8 iftype A <: U32 val => 11

class C is T
  fun g(): U8 => 2

class D[A: Any val] is T

primitive E is I2

actor Main
  new create(env: Env) =>
    let i: I = C
    if i.f[U32](1) != 1 then @pony_exitcode(1) end
    if i.f[U8](1) != 0 then @pony_exitcode(2) end
    let cj: (C & J) = C
    let i2: I = cj
    if i2.f[U32](1) != 1 then @pony_exitcode(3) end
    let i3: I = D[U32]
    if i3.f[U8](1) != 0 then @pony_exitcode(4) end
    if i3.f[U32](1) != 1 then @pony_exitcode(5) end
    let i4: IV = E
    if i4.f[U32](1) != 11 then @pony_exitcode(6) end
    if i4.f[U8](1) != 10 then @pony_exitcode(7) end
    let i5: IV = object is I2 end
    if i5.f[U32](1) != 11 then @pony_exitcode(8) end
    if i5.f[U8](1) != 10 then @pony_exitcode(9) end
