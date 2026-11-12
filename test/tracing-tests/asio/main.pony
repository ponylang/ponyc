use "runtime_tracing"

actor \nodoc\ Main
  new create(env: Env) =>
    let auth = RuntimeTracingAuth(env.root)
    let result =
      try
        match env.args(1)?
        | "enable" => RuntimeTracing.enable_categories(auth, [SchedulerTracing])
        | "disable" =>
          RuntimeTracing.disable_categories(auth, [SchedulerTracing])
        | "replace" => RuntimeTracing.set_categories(auth, [ActorTracing])
        | "sequence" =>
          RuntimeTracing.set_categories(auth, [SchedulerTracing])
          RuntimeTracing.disable_categories(auth, [SchedulerTracing])
          RuntimeTracing.enable_categories(auth, [SchedulerTracing])
        else
          env.exitcode(1)
          TracingNotAvailable
        end
      else
        env.exitcode(1)
        TracingNotAvailable
      end

    match result
    | TracingOk => None
    else env.exitcode(1)
    end
