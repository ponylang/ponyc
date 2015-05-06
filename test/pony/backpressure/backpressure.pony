actor Main
  let _env: Env

  new create(env: Env) =>
    _env = env
    foo("hello")

  be foo(msg: String) =>
    _env.out.print(msg)
    foo("world")
