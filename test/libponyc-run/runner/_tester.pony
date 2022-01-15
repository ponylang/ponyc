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
  var _start_ms: U64
  var _end_ms: U64

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
    _start_ms = 0
    _end_ms = 0
    start_building()

  be append_stdout(buf: Array[U8] iso) =>
    _out_buf.append(consume buf)

  be append_stderr(buf: Array[U8] iso) =>
    _err_buf.append(consume buf)

  be start_building() =>
    _env.out.print(_Colors.run(_definition.name))

    // find ponyc executable
    let ponyc_file_path =
      try
        _FindExecutable(_env, _options.ponyc)?
      else
        _shutdown_failed("unable to find executable: " + _options.ponyc)
        return
      end

    // build args for ponyc
    let args = _get_build_args(ponyc_file_path.path)

    if _options.verbose then
      let args_join = String
      for arg in args.values() do
        args_join.append(" ")
        args_join.append(arg)
      end
      _env.out.print(_Colors.info(_definition.name + ": building in: "
        + _definition.path))
      _env.out.print(_Colors.info(_definition.name + ": building: "
        + _options.ponyc + args_join))
    end

    // run ponyc to build the test program
    _start_ms = Time.millis()

    _stage = _Building
    _build_process = ProcessMonitor(_env.root, _env.root, _BuildProcessNotify(this),
      ponyc_file_path, consume args, _env.vars,
      FilePath(_env.root, _definition.path))
      .>done_writing()

  fun _get_build_args(ponyc_path: String): Array[String] val =>
    recover val
      let args = Array[String]
      args.push(ponyc_path)
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
      let test_fname =
        recover val
          let fname' = String
          fname'.append(Path.join(_options.output, _definition.name))
          ifdef windows then
            fname'.append(".exe")
          end
          fname'
        end

      if _options.verbose then
        _env.out.print(_Colors.info(_definition.name + ": testing: "
          + test_fname))
      end

      _out_buf.clear()
      _err_buf.clear()

      // run the test program
      let vars: Array[String val] val =
        recover val
          ifdef osx then
            let vars' = Array[String val](_env.vars.size() + 1)
            var found_dyld_library_path = false
            for v in _env.vars.values() do
              var pushed = false
              if v.contains("DYLD_LIBRARY_PATH") then
                found_dyld_library_path = true
                if not v.contains(_options.test_lib) then
                  vars'.push(v + ":" + _options.test_lib)
                  pushed = true
                end
              end
              if not pushed then vars'.push(v) end
            end
            if not found_dyld_library_path then
              vars'.push("DYLD_LIBRARY_PATH=" + _options.test_lib)
            end
            vars'
          else
            _env.vars
          end
        end

      ifdef osx then
        if _options.verbose then
          for v in vars.values() do
            if v.contains("DYLD_LIBRARY_PATH") then
              _env.out.print(_Colors.info(_definition.name + ": testing: " + v))
            end
          end
        end
      end

      _stage = _Testing
      _test_process = ProcessMonitor(_env.root, _env.root, _TestProcessNotify(this),
        FilePath(_env.root, test_fname), [ test_fname ], vars,
        FilePath(_env.root, _definition.path))
        .>done_writing()
    end

  be building_failed(msg: String) =>
    if _stage is _Building then
      _shutdown_failed("building failed: " + msg)
    end

  be testing_exited(exit_code: I32) =>
    if _stage is _Testing then
      if exit_code == _definition.expected_exit_code then
        _shutdown_succeeded()
      else
        _shutdown_failed("expected exit code "
          + _definition.expected_exit_code.string() + "; actual was "
          + exit_code.string())
      end
    end

  be testing_failed(msg: String) =>
    if _stage is _Testing then
      _shutdown_failed("testing failed: " + msg)
    end

  be timeout() =>
    if (_stage is _Building) or (_stage is _Testing) then
      _shutdown_failed("timed out after " + _options.timeout_s.string()
        + " seconds")
    end

  fun ref _shutdown_succeeded() =>
    if not (_stage is _Succeeded) then
      _end_ms = Time.millis()
      _env.out.print(_Colors.ok(_definition.name + " ("
        + (_end_ms - _start_ms).string() + " ms)"))
      _timers.cancel(_timer)
      _stage = _Succeeded
      _notify.succeeded(_definition.name)
    end

  fun ref _shutdown_failed(msg: String) =>
    if not (_stage is _Failed) then
      _end_ms = Time.millis()

      _env.out.print(_Colors.fail(_definition.name + " ("
        + (_end_ms - _start_ms).string() + " ms): " + msg))

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
    if _out_buf.size() > 0 then
      _env.out.print(_definition.name + ": STDOUT:\n"
        + recover val _out_buf.clone() end)
    end
    if _err_buf.size() > 0 then
      _env.out.print(_definition.name + ": STDERR\n"
        + recover val _err_buf.clone() end)
    end
