// Foo.f matches mk(), a (U8 | None), and returns from the U8 case. C's mk
// returns U8, so in C's copy the match always returns, but the code after it
// must not be reported as unreachable. C.f() must print "u".
trait Foo
  fun mk(): (U8 | None)
  fun f(): String =>
    match mk()
    | let u: U8 => return "u"
    end
    "none"
class C is Foo
  new create() => None
  fun mk(): U8 => 7
actor Main
  new create(env: Env) => env.out.print(C.f())
