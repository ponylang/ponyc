// Src.wrap captures self: Src[T0] val in a literal of Src[T0]. Nothing
// provides Src; the program must still compile and print nothing.
trait Src[T0: Any val]
  fun val get(): T0
  fun val wrap(f: {(T0): T0} val): Src[T0] val =>
    let self: Src[T0] val = this
    object val is Src[T0]
      fun val get(): T0 => f(self.get())
    end
actor Main
  new create(env: Env) => None
