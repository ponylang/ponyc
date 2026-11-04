use "runtime_tracing"
use "files"

actor Worker
  be work(env: Env) =>
    env.out.print("traced work done")

actor Main
  new create(env: Env) =>
    let auth = RuntimeTracingAuth(env.root)

    // enable_categories should succeed (log file opens successfully).
    match RuntimeTracing.enable_categories(
      auth, [SchedulerTracing; ActorBehaviorTracing])
    | TracingOk => None
    | TracingNotAvailable =>
      env.err.print("enable_categories: not available")
      env.exitcode(1)
      return
    | TracingLogFileError =>
      env.err.print("enable_categories: log file error")
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

    // Send work to another actor while tracing is active.
    let w = Worker
    w.work(env)

    // Enabling an already-enabled category should still succeed.
    match RuntimeTracing.enable_categories(auth, [SchedulerTracing])
    | TracingOk => None
    else
      env.err.print("re-enable failed")
      env.exitcode(1)
      return
    end

    // disable_categories should succeed.
    match RuntimeTracing.disable_categories(auth, [SchedulerTracing])
    | TracingOk => None
    else
      env.err.print("disable_categories failed")
      env.exitcode(1)
      return
    end

    // set_categories should succeed.
    match RuntimeTracing.set_categories(
      auth,
      [ ActorTracing; ActorBehaviorTracing
        ActorGCTracing; ActorStateChangeTracing ])
    | TracingOk => None
    else
      env.err.print("set_categories failed")
      env.exitcode(1)
      return
    end

    // Clearing all categories should succeed.
    match RuntimeTracing.set_categories(auth, [])
    | TracingOk => None
    else
      env.err.print("set_categories empty failed")
      env.exitcode(1)
      return
    end

    // Disabling when nothing is enabled should succeed.
    match RuntimeTracing.disable_categories(auth, [SchedulerTracing])
    | TracingOk => None
    else
      env.err.print("disable when empty failed")
      env.exitcode(1)
      return
    end
