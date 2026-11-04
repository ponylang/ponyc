use "runtime_tracing"

actor \nodoc\ Worker
  new create(env: Env) =>
    do_work(env)

  be do_work(env: Env) =>
    env.out.print("done")

actor \nodoc\ Main
  new create(env: Env) =>
    let auth = RuntimeTracingAuth(env.root)

    match \exhaustive\ RuntimeTracing.enable_categories(
      auth, [ActorTracing; ActorBehaviorTracing])
    | TracingOk => None
    | TracingNotAvailable =>
      env.err.print("ERROR: tracing not available")
      env.exitcode(1)
    | TracingLogFileError =>
      env.err.print("ERROR: could not open trace file")
      env.exitcode(1)
    end

    Worker(env)
