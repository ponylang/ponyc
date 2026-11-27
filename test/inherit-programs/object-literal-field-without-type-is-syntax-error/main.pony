// An object literal declares a field without a type. Object literal fields
// need a written type, so the program must fail with a syntax error at the
// field.
actor Main
  new create(env: Env) =>
    let o = object let x = U8(5) fun g(): U8 => x end
    env.out.print(o.g().string())
