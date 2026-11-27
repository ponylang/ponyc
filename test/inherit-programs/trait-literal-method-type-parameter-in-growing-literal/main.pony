// Src[A].pairwith[Z]() returns a Src[(A, Z)] literal that captures a: A. C is
// Src[U8], so C.pairwith[String]("s").get() must return (7, "s") and print
// "7s": the literal's type uses both the trait's and the method's type
// parameter, and the captured value's type must match its field.
trait Src[A: Any val]
  fun val get(): A
  fun val pairwith[Z: Any val](z: Z): Src[(A, Z)] val =>
    let a: A = get()
    object val is Src[(A, Z)]
      fun val get(): (A, Z) => (a, z)
    end
class val C is Src[U8]
  new val create() => None
  fun val get(): U8 => 7
actor Main
  new create(env: Env) =>
    (let x, let y) = C.pairwith[String]("s").get()
    env.out.print(x.string() + y)
