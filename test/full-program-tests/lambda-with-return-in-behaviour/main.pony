actor Main
  new create(env: Env) =>
    None

  be foo() =>
    let f = {() => if true then return None end}
