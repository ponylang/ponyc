use "collections"
use "files"
use "itertools"
use "process"
use "time"
use "backpressure"

interface tag _TesterNotify
  be print(name: String, str: String)
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
    _notify.print(_definition.name, _Colors.run(_definition.name))

    // find ponyc executable
    let ponyc_file_path =
      try
        _FindExecutable(_env, _options.ponyc)?
      else
        _shutdown_failed("unable to find executable: " + _options.ponyc)
        return
      end

    // build args for ponyc
    let args = _get_build_args(ponyc_file_path)

    if _options.verbose then
      let args_join = String
      for arg in args.values() do
        args_join.append(" ")
        args_join.append(arg)
      end
      _notify.print(_definition.name,
        _Colors.info(_definition.name + ": building in: " + _definition.path))
      _notify.print(_definition.name,
        _Colors.info(_definition.name + ": building:" + args_join))
    end

    // run ponyc to build the test program
    _start_ms = Time.millis()

    _stage = _Building
    let spa = StartProcessAuth(_env.root)
    let bpa = ApplyReleaseBackpressureAuth(_env.root)
    _build_process = ProcessMonitor(spa, bpa, _BuildProcessNotify(this),
      ponyc_file_path, consume args, _env.vars,
      FilePath(FileAuth(_env.root), _definition.path))
      .>done_writing()

  fun _get_build_args(ponyc_file_path: FilePath): Array[String] val =>
    recover val
      let args = Array[String]
      args.push(ponyc_file_path.path)
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

      _out_buf.clear()
      _err_buf.clear()

      // set up environment variables
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
              _notify.print(_definition.name,
                _Colors.info(_definition.name + ": testing: " + v))
            end
          end
        end
      end

      // set up args
      try
        (let executable_file_path: FilePath, let args: Array[String] val) =
          _get_debugger_and_args(test_fname)?

        if _options.verbose then
          let arg_join = String
          for arg in args.values() do
            arg_join.append(" ")
            arg_join.append(arg)
          end

          _notify.print(_definition.name,
            _Colors.info(_definition.name + ": testing:" + arg_join))
        end

        _stage = _Testing
        let spa = StartProcessAuth(_env.root)
        let bpa = ApplyReleaseBackpressureAuth(_env.root)
        _test_process = ProcessMonitor(spa, bpa, _TestProcessNotify(this),
          executable_file_path, args, vars,
          FilePath(FileAuth(_env.root), _definition.path))
          .>done_writing()
      end
    end

  fun ref _get_debugger_and_args(test_fname: String)
    : (FilePath, Array[String] val) ?
  =>
    recover
      let debugger_args = Array[String]
      let debugger_file_path =
        if _options.debugger.size() > 0 then
          try
            let debugger_split =
              recover val
                var debugger: String iso = _options.debugger.clone()
                debugger.replace("%20", " ")
                debugger.split(" ")
              end
            let debugger_fname = debugger_split(0)?

            var in_quote = false
            var cur_arg = String
            for fragment in debugger_split.trim(1).values() do
              if fragment.size() == 0 then continue end

              if in_quote then
                cur_arg.append(fragment)
                if fragment(fragment.size() - 1)? == '"' then
                  if (cur_arg(0)? == '"')
                    and (cur_arg(cur_arg.size() - 1)? == '"')
                  then
                    let quoted: String val = cur_arg.clone()
                    ifdef windows then
                      debugger_args.push(quoted)
                    else
                      debugger_args.push(quoted.trim(1, quoted.size() - 1))
                    end
                  else
                    debugger_args.push(cur_arg.clone())
                  end
                  cur_arg.clear()
                  in_quote = false
                else
                  cur_arg.append(" ")
                end
              else
                if fragment.contains("\"") then
                  cur_arg.append(fragment)
                  cur_arg.append(" ")
                  in_quote = true
                else
                  debugger_args.push(fragment)
                end
              end
            end

            _FindExecutable(_env, debugger_fname)?
          else
            _shutdown_failed("unable to find debugger: " + _options.debugger)
            error
          end
        end

      match debugger_file_path
      | let dfp: FilePath =>
        let args = Array[String]
        args.push(dfp.path)
        args.append(debugger_args)
        args.push(test_fname)
        (dfp, args)
      else
        (FilePath(FileAuth(_env.root), test_fname), [ test_fname ])
      end
    end

  be building_failed(msg: String) =>
    if _stage is _Building then
      _shutdown_failed("building failed: " + msg)
    end

  be testing_exited(exit_code: I32) =>
    if _stage is _Testing then
      var exit_code' = exit_code
      if _options.debugger.contains("lldb") then
        try
          let idx1 = _out_buf.find("exited with status = ")?
          let num = String
          var i = USize.from[ISize](idx1 + 21)
          while _out_buf(i)? != ' ' do
            num.push(_out_buf(i)?)
            i = i + 1
          end
          exit_code' = num.i32()?
        end
      end

      if exit_code' == _definition.expected_exit_code then
        _shutdown_succeeded()
      else
        _shutdown_failed("expected exit code "
          + _definition.expected_exit_code.string() + "; actual was "
          + exit_code'.string())
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
      _notify.print(_definition.name, _Colors.ok(_definition.name + " ("
        + (_end_ms - _start_ms).string() + " ms)"))
      _timers.cancel(_timer)
      _stage = _Succeeded
      _notify.succeeded(_definition.name)
    end

  fun ref _shutdown_failed(msg: String) =>
    if not (_stage is _Failed) then
      _end_ms = Time.millis()

      _notify.print(_definition.name, _Colors.fail(_definition.name + " ("
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
      _notify.print(_definition.name, _definition.name + ": STDOUT:\n"
        + recover val _out_buf.clone() end)
    end
    if _err_buf.size() > 0 then
      _notify.print(_definition.name, _definition.name + ": STDERR\n"
        + recover val _err_buf.clone() end)
    end
