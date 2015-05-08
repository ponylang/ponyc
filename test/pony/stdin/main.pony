class Notify is StdinNotify
  let _env: Env

  new iso create(env: Env) =>
    _env = env

  fun ref apply(data: Array[U8] iso): Bool =>
    let data' = consume val data

    try
      for c in data'.values() do
        _env.out.write(c.string(IntHex))
      end
    end
    _env.out.write("\n")

    true

actor Main
  new create(env: Env) =>
    env.input(Notify(env))
