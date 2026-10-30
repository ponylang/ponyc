use "runtime_tracing"
use "files"

actor Worker
  be work(env: Env) =>
    // Generates actor_behavior trace events when tracing is enabled.
    env.out.print("traced work done")

actor Main
  new create(env: Env) =>
    let auth = RuntimeTracingAuth(env.root)

    // enable_categories should return true (log file opens successfully).
    if not RuntimeTracing.enable_categories(
      auth, [SchedulerTracing; ActorBehaviorTracing])
    then
      env.err.print("enable_categories returned false")
      env.exitcode(1)
      return
    end

    // The trace output file should exist after enabling categories.
    let trace_path = FilePath(FileAuth(env.root), "ponytrace.json")
    try
      FileInfo(trace_path)?
    else
      env.err.print("trace file does not exist after enable")
      env.exitcode(1)
      return
    end

    // Trace all actors.
    RuntimeTracing.set_actor_tracing(auth, ActorTracingAll)

    // Send work to another actor while tracing is active.
    let w = Worker
    w.work(env)

    // Enabling an already-enabled category should still return true.
    if not RuntimeTracing.enable_categories(auth, [SchedulerTracing]) then
      env.err.print("re-enable returned false")
      env.exitcode(1)
      return
    end

    // disable_categories does not crash.
    RuntimeTracing.disable_categories(auth, [SchedulerTracing])

    // set_categories should return true.
    if not RuntimeTracing.set_categories(
      auth,
      [ ActorTracing; ActorBehaviorTracing
        ActorGCTracing; ActorStateChangeTracing ])
    then
      env.err.print("set_categories returned false")
      env.exitcode(1)
      return
    end

    // Clearing all categories should return true.
    if not RuntimeTracing.set_categories(auth, []) then
      env.err.print("set_categories empty returned false")
      env.exitcode(1)
      return
    end

    // Switch to no actor tracing.
    RuntimeTracing.set_actor_tracing(auth, ActorTracingNone)

    // Disabling when nothing is enabled should not crash.
    RuntimeTracing.disable_categories(auth, [SchedulerTracing])
