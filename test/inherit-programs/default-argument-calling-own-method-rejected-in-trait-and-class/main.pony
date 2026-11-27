// T's f() has a default argument that calls name(), which needs `this`.
// Expects "can't reference 'this' in a default argument" twice: once for T and
// once for C's copy, with a note naming method 'f' inherited by C from T.
trait T
  fun name(): String => "T"
  fun f(x: String = name()): String => x
class C is T
  fun name(): String => "C"
actor Main
  fun name(): String => "Main"
  new create(env: Env) => env.out.print(C.f())
