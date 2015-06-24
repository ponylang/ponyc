use "files"

actor Main
  new create(env: Env) =>
    try
      with file = File.open(FilePath(env.root, env.args(1))) do
        for line in file.lines() do
          env.out.print(line)
        end
      end
    else
      env.exitcode(-1)
    end
