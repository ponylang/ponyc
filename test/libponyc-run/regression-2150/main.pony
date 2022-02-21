// main.pony

actor Main
  new create(env: Env) =>
    let b = Bar[I32](123)
    b.bar()
