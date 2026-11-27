// A generic class, with no trait, passes a value of type A to a generic call
// whose type argument is inferred, inside iftype A <: Stringable.
// G[(P | None)] must take the else branch and print "other".
primitive P
primitive Show
  fun apply[S: Stringable val](s: S): String => s.string()
class G[A: Any val]
  let _a: A
  new create(a: A) => _a = a
  fun mk(): A => _a
  fun f(): String => iftype A <: Stringable val then Show(mk()) else "other" end
actor Main
  new create(env: Env) => env.out.print(G[(P | None)](P).f())
