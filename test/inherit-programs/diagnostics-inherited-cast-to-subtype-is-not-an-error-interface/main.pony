// I[U8]'s f() casts x to Stringable val, and in C's copy x is a U8, which is
// already Stringable. Expects the program to compile and print 7: the
// redundant cast is in inherited code from an interface, not reported.
interface I[A: Any val]
  fun f(x: A): String => try (x as Stringable val).string() else "no" end
class C is I[U8]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.f(7))
