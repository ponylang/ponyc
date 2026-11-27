// Interface Foo's f() matches x as U8 or String, and C fills A with U8, so
// in C's copy the String case can never match. Expects the program to compile
// and print "u": the dead case is in inherited code, which is not reported.
interface Foo[A: (U8 val | String val)]
  fun f(x: A): (String | None) =>
    match x
    | let u: U8 => "u"
    | let s: String => s
    end

class C is Foo[U8]
  new create() => None

actor Main
  new create(env: Env) =>
    env.out.print(C.f(3).string())
