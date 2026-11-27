// Src.pairwith captures get()'s result in a literal of Src[(A, Z)]. From an
// object literal of Src[U8], expects "7s": the capture keeps its type A.
trait Src[A: Any val]
  fun val get(): A
  fun val pairwith[Z: Any val](z: Z): Src[(A, Z)] val =>
    let a: A = get()
    object val is Src[(A, Z)]
      fun val get(): (A, Z) => (a, z)
    end
actor Main
  new create(env: Env) =>
    let c: Src[U8] val = object val is Src[U8]
      fun val get(): U8 => 7
    end
    (let x, let y) = c.pairwith[String]("s").get()
    env.out.print(x.string() + y)
