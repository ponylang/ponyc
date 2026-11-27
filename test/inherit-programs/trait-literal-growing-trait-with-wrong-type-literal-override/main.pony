// T[A]'s default m() builds a literal of T[Array[A] val]. Class H overrides
// m() with an object literal whose m() returns `this`, which has the wrong
// type. The test expects that one error at H's literal and no errors from T's.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
trait T2 is T[U8]
  fun m(): T[Array[U8] val] val
class val G is T2
  new val create() => None
  fun m(): T[Array[U8] val] val => H
  fun n(): String => "G"
class val H is T[Array[U8] val]
  new val create() => None
  fun m(): T[Array[Array[U8] val] val] val => object val is T[Array[Array[U8] val] val] fun n(): String => "x" fun m(): T[Array[Array[Array[U8] val] val] val] val => this end
  fun n(): String => "H"
actor Main
  new create(env: Env) => env.out.print(G.m().n())
