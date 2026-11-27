// Interface Foo's f() matches mk() against None, and C's mk() returns only
// U8, so in C's copy that pattern can never match. Expects the program to
// compile and print 3: the dead pattern is in inherited code, not reported.
interface Foo
  fun mk(): (U8 | None)
  fun f(): String =>
    match mk()
    | None => "none"
    | let u: U8 => u.string()
    end
class C is Foo
  new create() => None
  fun mk(): U8 => 3
actor Main
  new create(env: Env) => env.out.print(C.f())
