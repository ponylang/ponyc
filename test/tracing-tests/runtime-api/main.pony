use "runtime_tracing"

actor \nodoc\ Worker
  let _env: Env

  new create(env: Env) =>
    _env = env
    do_work()

  be do_work() =>
    _env.out.print("done")

actor \nodoc\ Main
  new create(env: Env) =>
    let auth = RuntimeTracingAuth(env.root)

    // Enable categories via the runtime API.
    RuntimeTracing.enable_categories(
      auth, [ActorTracing; ActorBehaviorTracing])

    Worker(env)

    // Disable all categories.
    RuntimeTracing.set_categories(auth, [])
