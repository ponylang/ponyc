// A generic class, with no trait, calls string() on a value of type A inside
// iftype A <: Stringable. G[(P | None)] must take the else branch and print
// "other", since P has no string(); G[U8] must print 5.
primitive P
class G[A: Any val]
  let _mk: {(): A} val
  new create(mk': {(): A} val) => _mk = mk'
  fun f(): String =>
    let m = _mk()
    iftype A <: Stringable val then m.string() else "other" end
actor Main
  new create(env: Env) =>
    env.out.print(G[(P | None)]({(): (P | None) => P}).f())
    env.out.print(G[U8]({(): U8 => 5}).f())
