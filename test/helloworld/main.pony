actor Main
  new create(env: Env) =>
    var str2: String = recover
      var str: String ref = recover String end

      for s in env.args.values() do
        str.append(s)
        str.append(" ")
      end

      consume str
    end

    env.stdout.print(recover String.concat(str2, "!\n") end)
