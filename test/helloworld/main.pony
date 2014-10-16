actor Main
  new create(env: Env) =>
    var str2: String = recover
      var str: String ref = String

      for s in env.args.values() do
        str.append(s)
        str.append(" ")
      end

      consume str
    end

    env.stdout.print(recover (str2 + "!").lower() end)
