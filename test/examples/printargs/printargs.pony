actor Main
  new create(env: Env) =>
    var str = recover String end

    try
      for s in env.args.values() do
        str.append(s)
        str.append(" ")
      end
    end

    env.stdout.print(consume str)
