class Notify is StdinNotify
  let _env: Env

  new iso create(env: Env) =>
    _env = env

  fun ref apply(data: Array[U8] iso): Bool =>
    let data' = consume val data

    try
      if data'(0) == 0x1B then
        return false
      end
    end

    _env.out.write(data')
    true

actor Main
  new create(env: Env) =>
    env.input(Notify(env))
