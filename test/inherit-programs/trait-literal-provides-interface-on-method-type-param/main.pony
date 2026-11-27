// Src.map returns an object literal that provides Getter[Z], where Z is map's
// own type parameter, and captures self and f. Expects 21.
interface val Getter[X: Any val]
  fun val get(): X
trait Src[T0: Any val]
  fun val get(): T0
  fun val map[Z: Any val](f: {(T0): Z} val): Getter[Z] val =>
    let self: Src[T0] val = this
    object val is Getter[Z]
      fun val get(): Z => f(self.get())
    end
class val Num is Src[U8]
  new val create() => None
  fun val get(): U8 => 20
actor Main
  new create(env: Env) => env.out.print(Num.map[String]({(x: U8): String => (x + 1).string()}).get())
