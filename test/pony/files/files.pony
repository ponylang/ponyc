use "files"

actor Main
  new create(env: Env) =>
    try
      with file = File.open(env.args(1)) do
        for line in file.lines() do
          env.out.write(line)
        end
      end
    else
      env.exitcode(-1)
    end