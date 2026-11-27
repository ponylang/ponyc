// Foo's f() casts mk() to U8, and C's mk() already returns U8, so in C's copy
// the cast is to the same type. Expects the program to compile and print 7:
// the redundant cast is in inherited code, which is not reported.
trait Foo
  fun mk(): (U8 | None)
  fun f(): U8 => try mk() as U8 else 0 end
class C is Foo
  new create() => None
  fun mk(): U8 => 7
actor Main
  new create(env: Env) => env.out.print(C.f().string())
