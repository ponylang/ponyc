// C's get() returns (String val | Array[U8] ref) where T says Any box. The
// Stringable box case in C's copy of f() must still compile; expects x.
trait T
  fun get(): Any box
  fun f(): String =>
    match get()
    | let s: Stringable box => s.string()
    else "o" end
class C is T
  new create() => None
  fun get(): (String val | Array[U8] ref) => "x"
actor Main
  new create(env: Env) => env.out.print(C.f())
