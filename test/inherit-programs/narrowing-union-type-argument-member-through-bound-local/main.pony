// G inherits f with A = (A2 | None). Binding mk() to a Stringable val local
// inside the iftype, then calling string() on the local, must compile. P is
// not Stringable and U8 is, so the program prints other then None.
primitive P
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    iftype A <: Stringable val then
      let v: Stringable val = mk()
      v.string()
    else "other" end
class G[A2: Any val] is Foo[(A2 | None)]
  new create() => None
  fun mk(): (A2 | None) => None
actor Main
  new create(env: Env) => env.out.print(G[P].f() + " " + G[U8].f())
