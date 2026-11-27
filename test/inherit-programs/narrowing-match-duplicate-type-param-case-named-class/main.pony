// Foo.f matches mk() against U8 and then A. C is Foo[U8], so both cases are
// U8 and the second cannot match. The program must compile and print u.
trait Foo[A: Any val]
  fun mk(): (A | U8)
  fun f(): String =>
    match mk()
    | let u: U8 => "u"
    | let a: A => "a"
    end
class C is Foo[U8]
  fun mk(): (U8 | U8) => U8(1)
actor Main
  new create(env: Env) =>
    env.out.print(C.f())
