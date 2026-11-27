// Each f copies a value of type (P & A) or (U8 & A) into a local and calls
// string() on it inside iftype A <: Stringable. G[(P | None)] prints other;
// H[U8] prints 3.
primitive P
class G[A: Any val]
  let _p: (P & A)
  new create(p: (P & A)) => _p = p
  fun mk(): (P & A) => _p
  fun f(): String =>
    let v = mk()
    iftype A <: Stringable val then v.string() else "other" end
class H[A: Any val]
  fun f(x: (U8 & A)): String =>
    let v = x
    iftype A <: Stringable val then v.string() else "other" end
actor Main
  new create(env: Env) =>
    env.out.print(G[(P | None)](P).f())
    env.out.print(H[U8].f(U8(3)))
