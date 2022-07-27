use lib = "lib"

actor Main
  new create(env: Env) =>
    let p = lib.Public.apply[U8]()
    env.out.print(p.string())
