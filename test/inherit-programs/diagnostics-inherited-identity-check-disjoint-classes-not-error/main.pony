// Foo's f() checks whether mk() is b, and C's mk() returns A1 while b is a
// B1, so in C's copy the check is always false. Expects the program to compile
// and print "diff": the always-false check is inherited code, not reported.
class A1
  new create() => None
class B1
  new create() => None
trait Foo
  fun mk(): (A1 | B1)
  fun f(b: B1): String => if mk() is b then "same" else "diff" end
class C is Foo
  new create() => None
  fun mk(): A1 => A1
actor Main
  new create(env: Env) => env.out.print(C.f(B1))
