// Interface Foo's f() wraps the partial g() in a try, and C's g() is not
// partial, so in C's copy both the `?` and the try are unneeded. Expects the
// program to compile and print 7: neither is reported in inherited code.
interface Foo
  fun g(): U8 ?
  fun f(): U8 => try g()? else 0 end

class C is Foo
  new create() => None
  fun g(): U8 => 7

actor Main
  new create(env: Env) =>
    env.out.print(C.f().string())
