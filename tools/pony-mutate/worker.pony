use "backpressure"
use "files"
use "process"
use "time"

class iso _MutantNotify is ProcessNotify
  """
  Captures exit status for a single mutant run.
  """
  let _worker: _Worker tag
  let _mutant: Mutant
  let _start_ns: U64

  new iso create(
    worker: _Worker tag,
    mutant: Mutant,
    start_ns: U64)
  =>
    _worker = worker
    _mutant = mutant
    _start_ns = start_ns

  fun ref dispose(
    process: ProcessMonitor ref,
    child_exit_status: ProcessExitStatus)
  =>
    let elapsed = Time.nanos() - _start_ns
    let result: MutantResult =
      match \exhaustive\ child_exit_status
      | let exited: Exited =>
        if exited.exit_code() == 0 then
          MutantSurvived
        else
          MutantKilled
        end
      | let signaled: Signaled =>
        MutantKilled
      end
    _worker._done(MutantOutcome(_mutant, result, elapsed))

  fun ref failed(process: ProcessMonitor ref, err: ProcessError) =>
    let elapsed = Time.nanos() - _start_ns
    _worker._done(MutantOutcome(_mutant, MutantKilled, elapsed))

actor _Worker
  """
  Runs one mutant at a time. Spawns the test binary with PONY_MUTANT_ID
  set, then reports the result back to the runner.
  """
  let _runner: Runner tag
  let _binary_path: FilePath
  let _args: Array[String] val
  let _base_vars: Array[String] val
  let _sp_auth: StartProcessAuth
  let _bp_auth: ApplyReleaseBackpressureAuth
  let _timeout_ns: U64
  var _monitor: (ProcessMonitor | None) = None
  var _timer: (Timer tag | None) = None
  var _timed_out: Bool = false
  let _timers: Timers

  new create(
    runner: Runner tag,
    binary_path: FilePath,
    args: Array[String] val,
    base_vars: Array[String] val,
    sp_auth: StartProcessAuth,
    bp_auth: ApplyReleaseBackpressureAuth,
    timeout_ns: U64,
    timers: Timers)
  =>
    _runner = runner
    _binary_path = binary_path
    _args = args
    _base_vars = base_vars
    _sp_auth = sp_auth
    _bp_auth = bp_auth
    _timeout_ns = timeout_ns
    _timers = timers

  be run(mutant: Mutant) =>
    _timed_out = false
    let vars = _make_vars(mutant.id)
    let notifier = _MutantNotify(this, mutant, Time.nanos())

    match \exhaustive\ StartProcess(
      _sp_auth,
      _bp_auth,
      consume notifier,
      _binary_path,
      _args,
      vars)
    | let pm: ProcessMonitor =>
      pm.done_writing()
      _monitor = pm
      if _timeout_ns > 0 then
        let mon: ProcessMonitor tag = pm
        let timer =
          Timer(
            _TimeoutNotify(this, mon),
            _timeout_ns,
            0)
        _timer = timer
        _timers(consume timer)
      end
    | let err: ProcessError =>
      _done(MutantOutcome(mutant, MutantKilled, 0))
    end

  be _timeout() =>
    _timed_out = true

  be _done(outcome: MutantOutcome) =>
    match _timer
    | let t: Timer tag => _timers.cancel(t)
    end
    _timer = None
    _monitor = None
    let outcome' =
      if _timed_out then
        MutantOutcome(outcome.mutant, MutantTimedOut, outcome.duration_ns)
      else
        outcome
      end
    _timed_out = false
    _runner._result(outcome', this)

  fun _make_vars(id: I32): Array[String] val =>
    recover val
      let v = Array[String](_base_vars.size() + 1)
      for s in _base_vars.values() do
        if not s.at("PONY_MUTANT_ID=") then
          v.push(s)
        end
      end
      v.push("PONY_MUTANT_ID=" + id.string())
      v
    end

class iso _TimeoutNotify is TimerNotify
  let _worker: _Worker tag
  let _monitor: ProcessMonitor tag

  new iso create(worker: _Worker tag, monitor: ProcessMonitor tag) =>
    _worker = worker
    _monitor = monitor

  fun ref apply(timer: Timer, count: U64): Bool =>
    _worker._timeout()
    _monitor.dispose()
    false
