use "options"

actor Main
  new create(env: Env) =>
    var str = recover String end

    for s in env.args.values() do
      str.append(s)
      str.append(" ")
    end

    env.out.print(consume str)

    let envvars = EnvVars(env.vars())

    for (k, v) in envvars.pairs() do
      env.out.print(k + " = " + v)
    end
