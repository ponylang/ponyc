// An object literal declares `let x = U32(3)` with no type. Fields must have a
// declared type, so the test expects the syntax error for the missing type.
actor Main
  new create(env: Env) =>
    let o = object
      let x = U32(3)
      fun get() => x
    end
    env.out.print((o.x + 1).string())
