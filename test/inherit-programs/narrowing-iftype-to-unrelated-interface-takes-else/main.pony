// Foo.g calls x.f() only when A <: HasS, whose f returns String rather than
// U8. X1 is not HasS, so Q's inherited g takes the else branch and prints 0.
interface val HasU
  fun f(): U8
interface val HasS
  fun f(): String
trait Foo[A: HasU val]
  fun g(x: A): U8 => iftype A <: HasS val then x.f() else 0 end
class val X1
  new val create() => None
  fun f(): U8 => 3
class Q is Foo[X1]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Q.g(X1).string())
