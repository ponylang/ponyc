actor Main
  new create(env: Env) =>
    let group_r = Array[F32].prealloc(8)
    let group_i = Array[F32].prealloc(8)

    for i in Range(0, 8) do
      group_r.append(0)
      group_i.append(0)
    end

    for j in Range(0, 8) do
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
