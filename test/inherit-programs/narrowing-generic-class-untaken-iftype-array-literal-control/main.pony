// A generic class, with no trait, builds an array of A values and calls
// string() on them inside iftype A <: Stringable. G[P] must take the else
// branch, since P has no string(), and print "other".
primitive P
class G[A: Any val]
  let _a: A
  new create(a: A) => _a = a
  fun mk(): A => _a
  fun f(): String =>
    iftype A <: Stringable val then
      let xs = [mk(); mk()]
      var s = String
      for x in xs.values() do s.append(x.string()) end
      s.clone()
    else "other" end
actor Main
  new create(env: Env) => env.out.print(G[P](P).f())
