actor Main
  new create(env: Env) =>
    var str = String

    for s in env.args.values() do
      str.append(s)
      str.append(" ")
    end

    env.stdout.print((str + "!").lower())
