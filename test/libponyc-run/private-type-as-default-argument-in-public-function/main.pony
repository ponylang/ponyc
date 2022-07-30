use lib = "lib"

actor Main
  new create(env: Env) =>
    lib.Public.apply[U8]()
