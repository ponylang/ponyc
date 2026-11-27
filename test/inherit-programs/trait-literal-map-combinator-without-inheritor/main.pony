// Src.map captures self: Src[A] val in a literal of Src[Z]. Nothing
// provides Src; the program must still compile and print nothing.
trait Src[A: Any val]
  fun val get(): A
  fun val map[Z: Any val](f: {(A): Z} val): Src[Z] val =>
    let self: Src[A] val = this
    object val is Src[Z]
      fun val get(): Z => f(self.get())
    end
actor Main
  new create(env: Env) => None
