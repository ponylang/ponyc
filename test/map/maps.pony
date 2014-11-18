use "collections"

actor Main
  new create(env: Env) =>
    var map = Map[String, U64]

    try
      for v in env.args.values() do
        map(v) = v.hash()
      end

      for (k, v) in map.pairs() do
        env.stdout.print(k + ": " + v.string())
      end
    end
