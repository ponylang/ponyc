// Src's type parameter is named T0 and map's is B. map captures
// self: Src[T0] val in a literal of Src[B]. Chaining two maps from Num must
// print 21.
trait Src[T0: Any val]
  fun val get(): T0
  fun val map[B: Any val](f: {(T0): B} val): Src[B] val =>
    let self: Src[T0] val = this
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
