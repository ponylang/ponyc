use @shim_has_ctx[I32]()

actor Main
  new create(env: Env) =>
    env.exitcode(@shim_has_ctx())
