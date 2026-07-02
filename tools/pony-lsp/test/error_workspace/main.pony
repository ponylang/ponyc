actor Main
  new create(env: Env) =>
    let fromb = Frombnicator.create("string")
    env.out.print(
      fromb.nicate().string()
    )
