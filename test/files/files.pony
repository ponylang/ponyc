use "files"

actor Main
  new create(env: Env) =>
    var map = Map[String, U64]

    try
      for v in env.args.values() do
        map.update(v, v.hash())
      end

      for v in env.args.values() do
        env.stdout.print(v + " = " + map(v).string())
      end
    end
    //
    // try
    //   var file = File.open(env.args(1))
    //
    //   for line in file.lines() do
    //     env.stdout.write(line)
    //   end
    //
    //   file.close()
    // end
