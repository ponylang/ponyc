use "files"
use "text"

actor Main
  new create(env: Env) =>
    try
      with file = File.open(env.args(1)) do
        var rope: Rope = recover Rope() end

        for line in file.lines() do
          rope = rope.concat(recover Rope(line) end)
        end

        env.out.print("Rope:")
        env.out.print("  Depth: " + rope.depth().string())
        env.out.print("  Size: " + rope.size().string())
        env.out.print("  Linebreaks: " + rope.linebreaks().string())
        env.out.print("  Blocksize: " + rope.blocksize().string())

        env.out.print(rope.string())
      end
    end
