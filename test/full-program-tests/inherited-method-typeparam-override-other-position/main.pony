// Classes override one of a trait's specializations. C guards the other type
// parameter position, so it adds a specialization instead of replacing the
// trait's. D and E write the trait's `and` and `or` clauses in another order,
// so they replace the trait's. F's guard has as many clauses as U's but tests
// other types, so it adds a specialization too.
use @pony_exitcode[None](code: I32)

trait T
  fun f[A: Any val, B: Any val](a: A, b: B): U8 => 0
  fun f[A: Any val, B: Any val](a: A, b: B): U8 iftype A <: U32 val => 1

trait U
  fun g[A: Any val, B: Any val](a: A, b: B): U8 => 0
  fun g[A: Any val, B: Any val](a: A, b: B): U8
    iftype A <: U32 val and B <: U8 val => 1

trait V[X: Any val]
  fun h(): U8 => 0
  fun h(): U8 iftype X <: U32 val or X <: U16 val => 1

class C is T
  fun f[B: Any val, A: Any val](a: B, b: A): U8 iftype A <: U32 val => 2

class D is U
  fun g[A: Any val, B: Any val](a: A, b: B): U8
    iftype B <: U8 val and A <: U32 val => 2

class E[X: Any val] is V[X]
  fun h(): U8 iftype X <: U16 val or X <: U32 val => 2

class F is U
  fun g[A: Any val, B: Any val](a: A, b: B): U8
    iftype B <: U8 val and A <: U16 val => 3

actor Main
  new create(env: Env) =>
    if C.f[U32, String](1, "x") != 1 then @pony_exitcode(1) end
    if C.f[String, U32]("x", 1) != 2 then @pony_exitcode(2) end
    let t: T = C
    if t.f[U32, String](1, "x") != 1 then @pony_exitcode(3) end
    if t.f[String, U32]("x", 1) != 2 then @pony_exitcode(4) end
    if D.g[U32, U8](1, 2) != 2 then @pony_exitcode(5) end
    if D.g[U8, U8](1, 2) != 0 then @pony_exitcode(6) end
    let u: U = D
    if u.g[U32, U8](1, 2) != 2 then @pony_exitcode(7) end
    if E[U32].h() != 2 then @pony_exitcode(8) end
    if E[U16].h() != 2 then @pony_exitcode(9) end
    if E[String].h() != 0 then @pony_exitcode(10) end
    let v: V[U32] = E[U32]
    if v.h() != 2 then @pony_exitcode(11) end
    if F.g[U32, U8](1, 2) != 1 then @pony_exitcode(12) end
    if F.g[U16, U8](1, 2) != 3 then @pony_exitcode(13) end
