actor Main
  new create(env: Env) =>
    var args = env.args
    for s in args.values() do
      env.stdout.print(s)
    end
