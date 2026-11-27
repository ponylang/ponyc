// T's literals provide U[A] and T[Array[A] val], and U's provides T[B], with
// T declared before U. Calls from C through both traits must print T.
trait T[A: Any val]
  fun n(): String => "T"
  fun a(): U[A] val => object val is U[A] end
  fun b(): T[Array[A] val] val => object val is T[Array[A] val] end
trait U[B: Any val]
  fun n(): String => "U"
  fun k(): T[B] val => object val is T[B] end
class C is T[U8]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.a().k().b().n())
