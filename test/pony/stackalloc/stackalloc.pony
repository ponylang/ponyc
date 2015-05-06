actor Main
  new create(env: Env) =>
    var t = "["
    var i: U32 = 1

    while i <= 4 do
      let x: String = i.string()
      t = t + x
      i = i + 1
    end

    t = t + "]"
    env.out.print(t)
