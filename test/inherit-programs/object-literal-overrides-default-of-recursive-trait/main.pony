// T[A].wrap returns an object literal of T[Array[A]]. An object literal in
// Main overrides n for T[String], and Leaf overrides wrap with its own literal
// that overrides n. The program must print "custom" and "wrapped leaf".
use "collections"
trait T[A: Any val]
  fun n(): String => "n"
  fun wrap(): T[Array[A] val] val => object val is T[Array[A] val] end
class val Leaf is T[U8]
  new val create() => None
  fun wrap(): T[Array[U8] val] val => object val is T[Array[U8] val] fun n(): String => "wrapped leaf" end
actor Main
  new create(env: Env) =>
    let t: T[String] val = object val is T[String] fun n(): String => "custom" end
    env.out.print(t.n())
    env.out.print(Leaf.wrap().n())
