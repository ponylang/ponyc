// An object literal in a generic method captures b, whose type is the method's
// type parameter B. P.m[U8](7) must build the literal and print 7.
primitive P
  fun m[B: Stringable val](b: B): Stringable val =>
    object val is Stringable
      fun string(): String iso^ => recover String .> append(b.string()) end
    end
actor Main
  new create(env: Env) =>
    env.out.print(P.m[U8](7).string())
