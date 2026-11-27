// For C, A is String, so the U8 case in C's copy of f() cannot match. The
// match must fall through to None, not be reported; expects none.
trait Foo[A: (U8 val | String val)]
  fun f(x: A): (String | None) =>
    match x
    | let u: U8 => "u"
    end

class C is Foo[String]
  new create() => None

actor Main
  new create(env: Env) =>
    match C.f("x")
    | None => env.out.print("none")
    | let s: String => env.out.print("str " + s)
    end
