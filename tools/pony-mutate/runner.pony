use "backpressure"
use "collections"
use "files"
use "process"
use "time"

actor Runner
  """
  Runs mutation tests: baseline first, then mutants in parallel with
  configurable concurrency. When a coverage file exists, mutants whose
  coverage counter was zero during the baseline are skipped.
  """
  let _env: Env
  let _binary_path: FilePath
  let _args: Array[String] val
  let _base_vars: Array[String] val
  let _all_mutants: Array[Mutant] val
  var _covered_mutants: Array[Mutant] val
  var _no_coverage_mutants: Array[Mutant] val
  let _num_workers: USize
  let _timeout_multiplier: U64
  let _reporter: Reporter
  let _json_file: (String val | None)
  let _sp_auth: StartProcessAuth
  let _bp_auth: ApplyReleaseBackpressureAuth
  let _timers: Timers
  var _outcomes: Array[MutantOutcome]
  var _next_idx: USize = 0
  var _completed: USize = 0
  var _active: USize = 0
  var _baseline_ns: U64 = 0
  var _workers: Array[_Worker tag]

  new create(
    env: Env,
    binary_path: FilePath,
    args: Array[String] val,
    base_vars: Array[String] val,
    mutants: Array[Mutant] val,
    num_workers: USize,
    timeout_multiplier: U64,
    reporter: Reporter,
    json_file: (String val | None) = None,
    do_baseline: Bool = true)
  =>
    """
    Start a mutation test run. Runs the baseline first (unless `do_baseline`
    is false), then dispatches mutants to workers.
    """
    _env = env
    _binary_path = binary_path
    _args = args
    _base_vars = base_vars
    _all_mutants = mutants
    _covered_mutants = mutants
    _no_coverage_mutants = recover val Array[Mutant] end
    _num_workers = num_workers
    _timeout_multiplier = timeout_multiplier
    _reporter = reporter
    _json_file = json_file
    _sp_auth = StartProcessAuth(env.root)
    _bp_auth = ApplyReleaseBackpressureAuth(env.root)
    _timers = Timers
    _outcomes = Array[MutantOutcome]
    _workers = Array[_Worker tag]

    if do_baseline then
      _run_baseline()
    else
      _baseline_ns = 1_000_000_000
      _start_mutants()
    end

  fun ref _run_baseline() =>
    let notifier = _BaselineNotify(this, Time.nanos())
    let vars = _strip_mutant_id(_base_vars)
    match \exhaustive\ StartProcess(
      _sp_auth,
      _bp_auth,
      consume notifier,
      _binary_path,
      _args,
      vars)
    | let pm: ProcessMonitor =>
      pm.done_writing()
    | let err: ProcessError =>
      _env.err.print(
        "error: cannot start baseline: " + err.string())
      _env.exitcode(1)
    end

  be _baseline_done(exit_code: I32, duration_ns: U64) =>
    if exit_code != 0 then
      _env.err.print(
        "error: baseline run failed (exit " + exit_code.string() +
          "). The test suite must pass before mutation testing.")
      _env.exitcode(1)
      return
    end

    _baseline_ns = duration_ns
    _env.err.print(
      "Baseline passed (" + _format_duration(duration_ns) + ")")

    let cov_path: String val = _binary_path.path + ".coverage"
    let auth = FileAuth(_env.root)
    match \exhaustive\ CoverageFile.read(cov_path, auth)
    | (let counters: Array[U64] val, CoverageOk) =>
      _filter_by_coverage(counters)
    | (let msg: String val, CoverageError) =>
      _env.err.print("warning: " + msg + " (running all mutants)")
    end

    _start_mutants()

  fun ref _filter_by_coverage(counters: Array[U64] val) =>
    let covered = recover iso Array[Mutant] end
    let uncovered = recover iso Array[Mutant] end
    for m in _all_mutants.values() do
      let idx = m.coverage_counter.usize()
      let hit =
        try counters(idx)? > 0
        else false
        end
      if hit then
        covered.push(m)
      else
        uncovered.push(m)
      end
    end
    _covered_mutants = consume covered
    _no_coverage_mutants = consume uncovered
    if _no_coverage_mutants.size() > 0 then
      _env.err.print(
        "Skipping " + _no_coverage_mutants.size().string() +
          " mutant(s) with no coverage")
    end

  fun ref _start_mutants() =>
    if _covered_mutants.size() == 0 then
      if _all_mutants.size() == 0 then
        _env.err.print("No mutants to run.")
      end
      _reporter.report(
        recover val Array[MutantOutcome] end,
        _no_coverage_mutants,
        _env)
      return
    end

    let timeout_ns = _baseline_ns * _timeout_multiplier
    let n = _num_workers.min(_covered_mutants.size())

    for i in Range[USize].till(0, n) do
      let w =
        _Worker(
          this,
          _binary_path,
          _args,
          _base_vars,
          _sp_auth,
          _bp_auth,
          timeout_ns,
          _timers)
      _workers.push(w)
    end

    for w in _workers.values() do
      _dispatch(w)
    end

  fun ref _dispatch(worker: _Worker tag) =>
    if _next_idx < _covered_mutants.size() then
      try
        let m = _covered_mutants(_next_idx)?
        _next_idx = _next_idx + 1
        _active = _active + 1
        worker.run(m)
      else
        _Unreachable()
      end
    end

  be _result(outcome: MutantOutcome, worker: _Worker tag) =>
    _active = _active - 1
    _completed = _completed + 1
    _outcomes.push(outcome)

    let status =
      match \exhaustive\ outcome.result
      | MutantKilled => "killed"
      | MutantSurvived => "SURVIVED"
      | MutantTimedOut => "timed out"
      end
    let m = outcome.mutant
    _env.err.print(
      "[" + _pad(_completed, _covered_mutants.size()) + "/" +
        _covered_mutants.size().string() + "] Mutant " +
        m.id.string() + " (" + m.description() +
        " at " + m.location() + ") " + status + " (" +
        _format_duration(outcome.duration_ns) + ")")

    if _next_idx < _covered_mutants.size() then
      _dispatch(worker)
    elseif _active == 0 then
      _finish()
    end

  fun ref _finish() =>
    _timers.dispose()
    let results: Array[MutantOutcome] iso =
      recover iso
        Array[MutantOutcome]
      end
    for o in _outcomes.values() do
      results.push(o)
    end
    let results': Array[MutantOutcome] val = consume results
    _reporter.report(results', _no_coverage_mutants, _env)
    match _json_file
    | let path: String val =>
      if not StrykerReporter.write_file(
        results', _no_coverage_mutants, path, _env)
      then
        _env.exitcode(1)
        return
      end
    end

    for o in results'.values() do
      if o.result is MutantSurvived then
        _env.exitcode(1)
        return
      end
    end

  fun _strip_mutant_id(vars: Array[String] val): Array[String] val =>
    recover val
      let v = Array[String]
      for s in vars.values() do
        if not s.at("PONY_MUTANT_ID=") then
          v.push(s)
        end
      end
      v
    end

  fun _pad(n: USize, total: USize): String val =>
    let ns: String val = n.string()
    let ts: String val = total.string()
    if ns.size() < ts.size() then
      recover val
        let s = String
        for _ in Range[USize].till(0, ts.size() - ns.size()) do
          s.push(' ')
        end
        s.append(ns)
        s
      end
    else
      ns
    end

  fun _format_duration(ns: U64): String val =>
    let secs = ns.f64() / 1e9
    if secs < 10.0 then
      let tenths = ((secs * 10.0) + 0.5).u64()
      (tenths / 10).string() + "." + (tenths % 10).string() + "s"
    else
      (secs + 0.5).u64().string() + "s"
    end

class iso _BaselineNotify is ProcessNotify
  let _runner: Runner tag
  let _start_ns: U64

  new iso create(runner: Runner tag, start_ns: U64) =>
    _runner = runner
    _start_ns = start_ns

  fun ref dispose(
    process: ProcessMonitor ref,
    child_exit_status: ProcessExitStatus)
  =>
    let elapsed = Time.nanos() - _start_ns
    let code: I32 =
      match \exhaustive\ child_exit_status
      | let exited: Exited => exited.exit_code()
      | let signaled: Signaled => -1
      end
    _runner._baseline_done(code, elapsed)

  fun ref failed(process: ProcessMonitor ref, err: ProcessError) =>
    let elapsed = Time.nanos() - _start_ns
    _runner._baseline_done(-1, elapsed)
