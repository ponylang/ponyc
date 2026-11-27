// Foo.f matches mk() against U8 and None. C's mk returns U8, so in C the None
// case cannot match. The program must compile and print 7.
trait Foo
  fun mk(): (U8 | None)
  fun f(): String =>
    match mk()
    | let u: U8 => u.string()
    | None => "none"
    end
class C is Foo
  new create() => None
  fun mk(): U8 => 7
actor Main
  new create(env: Env) => env.out.print(C.f())
