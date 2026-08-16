use "./pkg"

trait Middle is Base

actor Main is Middle
  new create(env: Env) =>
    hello(env)
