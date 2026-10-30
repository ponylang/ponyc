use "runtime_tracing"

actor Worker
  """
  A worker that runs a behavior while tracing is active.
  """
  let _env: Env

  new create(env: Env) =>
    _env = env
    do_work()

  be do_work() =>
    _env.out.print("Worker running")

actor Main
  new create(env: Env) =>
    let auth = RuntimeTracingAuth(env.root)

    // Enable scheduler and actor behavior tracing.
    RuntimeTracing.enable_categories(
      auth, [SchedulerTracing; ActorBehaviorTracing])

    // Trace all actors.
    RuntimeTracing.set_actor_tracing(auth, ActorTracingAll)

    // Spawn a worker.
    Worker(env)

    // Narrow the active set to just actor categories.
    RuntimeTracing.set_categories(
      auth,
      [ ActorTracing; ActorBehaviorTracing
        ActorGCTracing; ActorStateChangeTracing ])

    // Switch to cycle detector only.
    RuntimeTracing.set_actor_tracing(auth, ActorTracingCycleDetectorOnly)

    // Turn off all tracing.
    RuntimeTracing.set_categories(auth, [])
