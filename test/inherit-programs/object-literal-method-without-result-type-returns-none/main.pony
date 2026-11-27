// An object literal's method has no result type, so it returns None, and the
// program adds 1 to its result. The test expects "couldn't find 'add' in
// 'None'", the language rule for methods without a result type.
actor Main
  new create(env: Env) =>
    let o = object
      fun get(): U32 => 3
      fun twice() => get() * 2
    end
    env.out.print((o.twice() + 1).string())
