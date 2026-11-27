// T2 provides T1 and redeclares f() with no body, and C provides T2. The test
// expects "no body found for method 'f'": T2's declaration replaces T1's
// default, so C has no body to inherit.
trait T1
  fun f(): String => "T1 body"

trait T2 is T1
  fun f(): String

class C is T2

actor Main
  new create(env: Env) =>
    env.out.print(C.f())
