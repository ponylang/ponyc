actor Main
  new create(env: Env) =>
    let group_r = Array[F32].init(0, 8)
    let group_i = Array[F32].init(0, 8)

    for j in Range[U64](0, 8) do
      var r: F32 = 0.0
      var i: F32 = 0.0

      try
        r = group_r(j)
        i = group_i(j)
      else
        env.stdout.print("index error")
        break None
      end
    end
