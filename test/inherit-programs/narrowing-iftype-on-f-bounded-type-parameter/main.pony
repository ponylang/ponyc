// Foo's A is bounded by Selfy[A]. Bar fills it with D, which is not
// Stringable, and Baz with E, which is. Expects "otherE": the iftype branch
// runs only in Baz's copy, and calling string() on y in Bar's copy is no error.
interface val Selfy[S]
  fun val me(): S
trait Foo[A: Selfy[A] val]
  fun f(x: A): String =>
    let y = x.me()
    iftype A <: Stringable val then y.string() else "other" end
class val D is Selfy[D]
  new val create() => None
  fun val me(): D => this
class val E is (Selfy[E] & Stringable)
  new val create() => None
  fun val me(): E => this
  fun string(): String iso^ => "E".clone()
class Bar is Foo[D]
  new create() => None
class Baz is Foo[E]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.f(D) + Baz.f(E))
