// A lambda with no result type returns None, and an object literal in the
// same method has a method returning 3. The program must print "None3".
actor Main
  new create(env: Env) =>
    let f = {(x: U8) => x + 1}
    let o = object
      fun g(): U8 => 3
    end
    env.out.print(f(3).string() + o.g().string())
