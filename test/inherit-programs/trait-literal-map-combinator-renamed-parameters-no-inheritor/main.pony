// Src.map, with type parameters named T0 and Z, captures self: Src[T0] val
// in a literal of Src[Z]. Nothing provides Src; the program must still
// compile and print nothing.
trait Src[T0: Any val]
  fun val get(): T0
  fun val map[Z: Any val](f: {(T0): Z} val): Src[Z] val =>
    let self: Src[T0] val = this
    object val is Src[Z]
      fun val get(): Z => f(self.get())
    end
actor Main
  new create(env: Env) => None
