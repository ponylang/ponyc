// Foo's f() matches mk() with an else clause, and C's mk() returns only U8,
// so in C's copy the match is exhaustive. Expects the program to compile and
// print 3: the unreachable else is in inherited code, which is not reported.
interface Foo
  fun mk(): (U8 | None)
  fun f(): String =>
    match mk()
    | let u: U8 => u.string()
    else "other" end
class C is Foo
  new create() => None
  fun mk(): U8 => 3
actor Main
  new create(env: Env) => env.out.print(C.f())
