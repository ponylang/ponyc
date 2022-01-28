actor Main
  new create(env: Env) =>
    let f = {() => if true then return None end}
