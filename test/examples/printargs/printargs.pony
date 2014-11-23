actor Main
  new create(env: Env) =>
    var str = recover String end

    try
      for s in env.args.values() do
        str.append(s)
        str.append(" ")
      end
    end

    env.out.println(consume str)
