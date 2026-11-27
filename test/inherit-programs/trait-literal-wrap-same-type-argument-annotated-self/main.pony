// Src.wrap captures self: Src[T0] val in a literal of Src[T0] that applies
// f to self.get(). Wrapping Num's 20 with an add-one lambda must print 21.
trait Src[T0: Any val]
  fun val get(): T0
  fun val wrap(f: {(T0): T0} val): Src[T0] val =>
    let self: Src[T0] val = this
    object val is Src[T0]
      fun val get(): T0 => f(self.get())
    end
class val Num is Src[U8]
  new val create() => None
  fun val get(): U8 => 20
actor Main
  new create(env: Env) => env.out.print(Num.wrap({(x: U8): U8 => x + 1}).get().string())
