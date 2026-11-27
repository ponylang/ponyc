// C inherits f with A = P, which is not Stringable, so the then branch
// calling Show(x) can't run. Binding the iftype's result to a local with an
// inferred type must compile, and the program prints the else value s.
primitive P
primitive Show
  fun apply[T: Stringable val](t: T): String => t.string()
trait Foo[A: Any val]
  fun f(x: A): String =>
    let v = iftype A <: Stringable val then Show(x) else "s" end
    v
class C is Foo[P]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.f(P))
