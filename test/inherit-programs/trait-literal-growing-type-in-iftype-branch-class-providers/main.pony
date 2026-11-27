// T[A].m() builds a T[Array[A] val] literal only when A is Stringable. C is
// T[U8], which takes that branch and prints "n"; D is T[Array[U8] val], which
// takes the else branch and prints "other". The inherited copies must keep
// the iftype narrowing and type the growing literal with each class's A.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): String => iftype A <: Stringable val then (object val is T[Array[A] val] end).n() else "other" end
class val C is T[U8]
  new val create() => None
class val D is T[Array[U8] val]
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(C.m() + " " + D.m())
