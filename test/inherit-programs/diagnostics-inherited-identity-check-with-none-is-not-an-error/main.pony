// Foo's f() checks whether mk() is None, and C's mk() returns only U8, so in
// C's copy the comparison is always false. Expects the program to compile and
// print "some": the always-false check is in inherited code, not reported.
trait Foo
  fun mk(): (U8 | None)
  fun f(): String => if mk() is None then "none" else "some" end
class C is Foo
  new create() => None
  fun mk(): U8 => 3
actor Main
  new create(env: Env) => env.out.print(C.f())
