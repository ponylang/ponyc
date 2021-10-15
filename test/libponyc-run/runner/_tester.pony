use "collections"
use "files"
use "itertools"
use "process"
use "time"

interface tag _TesterNotify
  be succeeded(name: String)
  be failed(name: String)

primitive _NotStarted
primitive _Building
primitive _Testing
primitive _Succeeded
primitive _Failed

type _TesterStage is (_NotStarted | _Building | _Testing | _Succeeded | _Failed)

actor _Tester
  let _env: Env
  let _timers: Timers
  let _options: _Options
  let _definition: _TestDefinition
  let _notify: _TesterNotify

  var _stage: _TesterStage
  let _timer: Timer tag

  var _build_process: (ProcessMonitor | None) = None
  var _test_process: (ProcessMonitor | None) = None

  let _out_buf: String iso = recover String end
  let _err_buf: String iso = recover String end

  new create(env: Env, timers: Timers, options: _Options,
    definition: _TestDefinition, notify: _TesterNotify)
  =>
    _env = env
    _timers = timers
    _options = options
    _definition = definition
    _notify = notify

    _stage = _NotStarted
    let timer = Timer(_TesterTimerNotify(this),
      _options.timeout_s * 1_000_000_000)
    _timer = timer
    _timers(consume timer)
    start_building()

  be append_stdout(buf: Array[U8] iso) =>
    _out_buf.append(consume buf)

  be append_stderr(buf: Array[U8] iso) =>
    _err_buf.append(consume buf)

  be start_building() =>
    // find ponyc executable
    let ponyc_file_path =
      try
        _FindExecutable(_env, _options.ponyc)?
      else
        _env.err.print(_definition.name
          + ": FAILED: unable to find executable " + _options.ponyc)
        _shutdown_failed()
        return
      end

    // build args for ponyc
    let args = _get_build_args()

    // add PONYPATH to vars if necessary
    if _options.verbose then
      let args_join = String
      for arg in args.values() do
        args_join.append(" ")
        args_join.append(arg)
      end
      _env.out.print(_definition.name + ": building: working dir: " + _definition.path)
      _env.out.print(_definition.name + ": building: " + _options.ponyc + args_join)
    end

    // run ponyc to build the test program
    try
      let auth = _env.root as AmbientAuth
      _stage = _Building
      _build_process = ProcessMonitor(auth, auth, _BuildProcessNotify(this),
        ponyc_file_path, consume args, _env.vars,
        FilePath(auth, _definition.path))
        .>done_writing()
    else
      _env.err.print(_definition.name + ": failed to acquire ambient auth")
      _shutdown_failed()
    end

  fun _get_build_args(): Array[String] val =>
    recover val
      let args = Array[String]
      if _options.debug then
        args.push("--debug")
      end
      if _options.test_lib.size() > 0 then
        args.push("--path=" + _options.test_lib)
      end
      if _options.pony_path.size() > 0 then
        for path in _options.pony_path.split(Path.list_sep()).values() do
          args.push("--path=" + path)
        end
      end
      args.push("--output=" + _options.output)
      args.push(_definition.path)
      args
    end

  be building_succeeded() =>
    if _stage is _Building then
      let fname =
        recover val
          let fname' = String
          fname'.append(Path.join(_options.output, _definition.name))
          ifdef windows then
            fname'.append(".exe")
          end
          fname'
        end

      if _options.verbose then
        _env.out.print(_definition.name + ": testing: " + fname)
      end

      _out_buf.clear()
      _err_buf.clear()

      // run the test program
      try
        let auth = _env.root as AmbientAuth
        _stage = _Testing
        _test_process = ProcessMonitor(auth, auth, _TestProcessNotify(this),
          FilePath(auth, fname), [], _env.vars,
          FilePath(auth, _definition.path))
          .>done_writing()
      else
        _env.err.print(_definition.name + ": failed to acquire ambient auth")
        _shutdown_failed()
      end
    end

  be building_failed(msg: String) =>
    if _stage is _Building then
      _env.err.print(_definition.name + ": BUILD FAILED: " + msg)
      _shutdown_failed()
    end

  be testing_exited(exit_code: I32) =>
    if _stage is _Testing then
      if exit_code == _definition.expected_exit_code then
        _shutdown_succeeded()
      else
        _env.err.print(_definition.name + ": TEST FAILED: expected exit code "
          + _definition.expected_exit_code.string() + "; actual was "
          + exit_code.string())
        _shutdown_failed()
      end
    end

  be testing_failed(msg: String) =>
    if _stage is _Testing then
      _env.err.print(_definition.name + ": TEST FAILED: " + msg)
      _shutdown_failed()
    end

  be timeout() =>
    if (_stage is _Building) or (_stage is _Testing) then
      _env.err.print(_definition.name + ": TIMED OUT after "
        + _options.timeout_s.string() + " seconds")
      _shutdown_failed()
    end

  fun ref _shutdown_succeeded() =>
    if not (_stage is _Succeeded) then
      _timers.cancel(_timer)
      _stage = _Succeeded
      _notify.succeeded(_definition.name)
      _env.out.print(_definition.name + ": SUCCEEDED")
    end

  fun ref _shutdown_failed() =>
    if not (_stage is _Failed) then
      match _build_process
      | let process: ProcessMonitor =>
        process.dispose()
        _build_process = None
      end
      match _test_process
      | let process: ProcessMonitor =>
        process.dispose()
        _test_process = None
      end

      _dump_io_streams()
      _timers.cancel(_timer)
      _stage = _Failed
      _notify.failed(_definition.name)
    end

  fun ref _dump_io_streams() =>
    if _options.verbose and (_out_buf.size() > 0) then
      _env.out.print(_definition.name + ": STDOUT:\n"
        + recover val _out_buf.clone() end)
    end
    if _err_buf.size() > 0 then
      _env.err.print(_definition.name + ": STDERR\n"
        + recover val _err_buf.clone() end)
    end
