use "files"
use "text"

actor Main
  new create(env: Env) =>
    try
      var file = File.open(env.args(1))
      var rope: Rope val = recover Rope() end

      for line in file.lines() do
        rope = rope.concat(recover Rope(line) end)
      end

      env.stdout.print("Rope:")
      env.stdout.print("  Depth: " + rope.depth().string())
      env.stdout.print("  Size: " + rope.size().string())
      env.stdout.print("  Linebreaks: " + rope.linebreaks().string())
      env.stdout.print("  Blocksize: " + rope.blocksize().string())
      //env.stdout.print("  Balanced: " + rope.balanced().string())

      var i: U64 = 0

      env.stdout.print(">>> Test RopeLines Iterator <<<")
      for l in rope.lines() do
        env.stdout.write("[Line: " + i.string() + "] " + l)
        i = i + 1
      end
      env.stdout.print(">>> End Test RopeLines Iterator <<<")

      env.stdout.print(">>> Test Stringable <<<")
      env.stdout.write(rope.string())
      env.stdout.print(">>> End Test Stringable <<<")

      let cursor = env.args(2).u64()

      env.stdout.print(">>> Test cursor position <<<")
      let pos = rope.position(cursor)
      env.stdout.print("cursor: " + cursor.string() + " " + env.args(1) + ":" +
        pos._1.string() + ":" + pos._2.string())
      env.stdout.print(">>> End test cursor position <<<")
    end
