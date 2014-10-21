actor Main
  new create(env: Env) =>
    var str = String

    try
      for s in env.args.values() do
        str.append(s)
        str.append(" ")
      end
    end

    env.stdout.print((str + "!").lower())
