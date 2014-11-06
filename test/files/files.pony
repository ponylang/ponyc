use "files"

actor Main
  new create(env: Env) =>
    try
      var path = Path.abs("README")
      env.stdout.print(path)

      var file = File.open(path)

      for line in file.lines() do
        env.stdout.write(line)
      end
    end
