actor Main
  new create(env: Env) =>
    let a: F64 = 3.14
    let b: F64 = a.powi(-2)
    env.out.print("B is now " + b.string())

    let c: F32 = 3.14
    let d: F32 = c.powi(-3)
    env.out.print("D is now " + d.string())
