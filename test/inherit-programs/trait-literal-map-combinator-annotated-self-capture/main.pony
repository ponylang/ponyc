// Src.map captures this as self: Src[A] val and returns a literal of Src[B]
// that applies f to self.get(). Chaining two maps from Num must print 21.
// See #6736.
trait Src[A: Any val]
  fun val get(): A
  fun val map[B: Any val](f: {(A): B} val): Src[B] val =>
    let self: Src[A] val = this
    object val is Src[B]
      fun val get(): B => f(self.get())
    end
class val Num is Src[U8]
  new val create() => None
  fun val get(): U8 => 20
actor Main
  new create(env: Env) =>
    let s = Num.map[U16]({(x: U8): U16 => x.u16() + 1}).map[String]({(y: U16): String => y.string()})
    env.out.print(s.get())
