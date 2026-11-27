// Foo's f() wraps the partial mk() in a try, and C's mk() is not partial, so
// in C's copy the try never errors. Expects the program to compile and print
// 3: the unneeded try is in inherited code, which is not reported.
trait Foo
  fun mk(): U8 ?
  fun f(): U8 => try mk()? else 0 end
class C is Foo
  fun mk(): U8 => 3
actor Main
  new create(env: Env) =>
    let o = C
    env.out.print(o.f().string())
