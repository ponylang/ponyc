// rot() and swp() reorder T's six type arguments; together they reach all
// 720 orders. The program calls C.rot().swp().rot(); the test expects it to
// compile within the runner's time limit and print nothing.
trait T[A1, A2, A3, A4, A5, A6]
  fun rot(): T[A2, A3, A4, A5, A6, A1] val => object is T[A2, A3, A4, A5, A6, A1] end
  fun swp(): T[A2, A1, A3, A4, A5, A6] val => object is T[A2, A1, A3, A4, A5, A6] end
class C is T[U8, U16, U32, U64, I8, I16]
  new create() => None
actor Main
  new create(env: Env) => C.rot().swp().rot()
