use d = "./dep"

actor Main is d.Gen
  new create(env: Env) =>
    hello()

  fun tag hello(): None => None
