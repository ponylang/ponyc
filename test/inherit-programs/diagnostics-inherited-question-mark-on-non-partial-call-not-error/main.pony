// Interface Foo's partial f() calls g() with `?`, and C's g() is not
// partial. Expects the program to compile and print 7: the unneeded `?` is in
// inherited code, which is not reported.
interface Foo
  fun g(): U8 ?
  fun f(): U8 ? => g()?
class C is Foo
  new create() => None
  fun g(): U8 => 7
actor Main
  new create(env: Env) => try env.out.print(C.f()?.string()) end
