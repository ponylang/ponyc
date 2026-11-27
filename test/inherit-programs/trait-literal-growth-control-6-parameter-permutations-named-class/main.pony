// A trait with 6 type parameters declares rot() and swp(), which return the
// trait with its arguments permuted, and named class R implements them. The
// permutations form a finite set. The test expects "ok".
trait T[A1, A2, A3, A4, A5, A6]
  fun rot(): T[A2, A3, A4, A5, A6, A1] val
  fun swp(): T[A2, A1, A3, A4, A5, A6] val
class val R[A1, A2, A3, A4, A5, A6] is T[A1, A2, A3, A4, A5, A6]
  new val create() => None
  fun rot(): T[A2, A3, A4, A5, A6, A1] val => R[A2, A3, A4, A5, A6, A1]
  fun swp(): T[A2, A1, A3, A4, A5, A6] val => R[A2, A1, A3, A4, A5, A6]
class C is T[U8, U16, U32, U64, I8, I16]
  new create() => None
  fun rot(): T[U16, U32, U64, I8, I16, U8] val => R[U16, U32, U64, I8, I16, U8]
  fun swp(): T[U16, U8, U32, U64, I8, I16] val => R[U16, U8, U32, U64, I8, I16]
actor Main
  new create(env: Env) => C.rot().swp().rot(); env.out.print("ok")
