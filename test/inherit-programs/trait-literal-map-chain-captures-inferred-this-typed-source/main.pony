// Src.map captures this in an inferred local and returns a literal of Src[B].
// Two maps on a source typed Src[U8] val expect "21": each level's capture
// works although the levels have different type arguments.
trait Src[A: Any val]
  fun val get(): A
  fun val map[B: Any val](f: {(A): B} val): Src[B] val =>
    let self = this
    object val is Src[B]
      fun val get(): B => f(self.get())
    end
actor Main
  new create(env: Env) =>
    let n: Src[U8] val = object val is Src[U8]
      fun val get(): U8 => 20
    end
    let s = n.map[U16]({(x: U8): U16 => x.u16() + 1}).map[String]({(y: U16): String => y.string()})
    env.out.print(s.get())
