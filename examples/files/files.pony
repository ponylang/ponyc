use "files"

actor Main
  new create(env: Env) =>
    let caps = recover val FileCaps.set(FileRead).set(FileStat) end

    try
      with file = OpenFile(FilePath(env.root, env.args(1), caps)) as File do
        env.out.print(file.path.path)
        for line in file.lines() do
          env.out.print(line)
        end
      end
    else
      try
        env.out.print("Couldn't open " + env.args(1))
      end
    end
