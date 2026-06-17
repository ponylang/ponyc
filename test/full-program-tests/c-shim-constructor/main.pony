use @shim_constructor_ran[I32]()

actor Main
  new create(env: Env) =>
    env.exitcode(@shim_constructor_ran())
