use "files"

actor Main
  new create(env: Env) =>
    try
      var file = File.open(env.args(1))

      for line in file.lines() do
        env.out.print(line)
      end

      file.close()
    end
