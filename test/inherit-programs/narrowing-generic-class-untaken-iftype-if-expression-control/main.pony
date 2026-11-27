// A generic class, with no trait, takes a value of type A from an if
// expression and calls string() on it inside iftype A <: Stringable. G[P]
// must take the else branch, since P has no string(), and print "other".
primitive P
class G[A: Any val]
  let _a: A
  new create(a: A) => _a = a
  fun mk(): A => _a
  fun f(b: Bool): String =>
    iftype A <: Stringable val then
      let x = if b then mk() else mk() end
      x.string()
    else "other" end
actor Main
  new create(env: Env) => env.out.print(G[P](P).f(true))
