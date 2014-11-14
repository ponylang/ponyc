use "files"

actor Main
  new create(env: Env) =>
    try
      var i: U64 = 1

      while i < env.args.length() do
        env.stdout.print(Path.clean(env.args(i)))
        // var r = Path.rel(env.args(1), env.args(2))
        // var j = Path.join(env.args(1), r)
        // env.stdout.print(r)
        // env.stdout.print(j)
        i = i + 1
      end
    end

    // try
    //   var file = File.open(env.args(1))
    //
    //   for line in file.lines() do
    //     env.stdout.write(line)
    //   end
    //
    //   file.close()
    // end
