actor \nodoc\ Worker
  let _env: Env

  new create(env: Env) =>
    _env = env
    do_work()

  be do_work() =>
    _env.out.print("done")

actor \nodoc\ Main
  new create(env: Env) =>
    Worker(env)
