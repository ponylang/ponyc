// Interface I's f() calls the partial g() with `?`, and C's g() is not
// partial. Expects the program to compile and print 1: the unneeded `?` is in
// inherited code, which is not reported.
interface I
  fun g(): U8 ?
  fun f(): U8 ? => g()?
class C is I
  new create() => None
  fun g(): U8 => 1
actor Main
  new create(env: Env) => try env.out.print(C.f()?.string()) end
