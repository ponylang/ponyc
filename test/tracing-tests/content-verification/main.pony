use "runtime_tracing"

actor _Worker
  let _coordinator: _Coordinator

  new create(coordinator: _Coordinator) =>
    _coordinator = coordinator

  be ping(peer: _Worker, ttl: U32) =>
    if ttl == 0 then
      _coordinator.round_done()
    else
      peer.ping(this, ttl - 1)
    end

actor _Coordinator
  let _env: Env
  let _auth: RuntimeTracingAuth
  let _workers: Array[_Worker tag] val
  var _rounds_remaining: U32 = 0
  var _phase: U32 = 0

  new create(env: Env) =>
    _env = env
    _auth = RuntimeTracingAuth(env.root)

    let ws = recover iso Array[_Worker tag](4) end
    var i: U32 = 0
    while i < 4 do
      ws.push(_Worker(this))
      i = i + 1
    end
    _workers = consume ws

    _start_phase(1)

  fun ref _start_phase(phase: U32) =>
    _phase = phase
    match phase
    | 1 =>
      match RuntimeTracing.enable_categories(
        _auth, [ActorBehaviorTracing])
      | TracingOk => None
      else
        _env.err.print("phase 1: enable_categories failed")
        _env.exitcode(1)
        return
      end
    | 2 =>
      match RuntimeTracing.set_categories(
        _auth, [SchedulerStealingTracing])
      | TracingOk => None
      else
        _env.err.print("phase 2: set_categories failed")
        _env.exitcode(1)
        return
      end
    | 3 =>
      match RuntimeTracing.set_categories(_auth, [])
      | TracingOk => None
      else
        _env.err.print("phase 3: set_categories failed")
        _env.exitcode(1)
        return
      end
    | 4 =>
      match RuntimeTracing.enable_categories(
        _auth, [ActorBehaviorTracing])
      | TracingOk => None
      else
        _env.err.print("phase 4: re-enable failed")
        _env.exitcode(1)
        return
      end
    | 5 =>
      match RuntimeTracing.set_categories(_auth, [])
      | TracingOk => None
      else
        _env.err.print("cleanup: disable failed")
        _env.exitcode(1)
        return
      end
      return
    end
    _rounds_remaining = 2000
    _fire_one()

  fun ref _fire_one() =>
    try
      let idx = _rounds_remaining.usize() % _workers.size()
      let peer_idx = (idx + 1) % _workers.size()
      _workers(idx)?.ping(_workers(peer_idx)?, 20)
    end

  be round_done() =>
    if _rounds_remaining == 0 then
      return
    end
    _rounds_remaining = _rounds_remaining - 1
    if _rounds_remaining == 0 then
      _start_phase(_phase + 1)
    else
      _fire_one()
    end

actor \nodoc\ Main
  new create(env: Env) =>
    _Coordinator(env)
