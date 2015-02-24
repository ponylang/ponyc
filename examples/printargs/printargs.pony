use "options"

actor Main
  new create(env: Env) =>
    var str = recover String end

    try
      for s in env.args.values() do
        str.append(s)
        str.append(" ")
      end
    end

    env.out.print(consume str)

    let envvars = EnvVars(env.vars)

    try
      for (k, v) in envvars.pairs() do
        env.out.print(k + " = " + v)
      end
    end
