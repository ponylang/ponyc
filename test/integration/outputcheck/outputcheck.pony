use "process"
use "files"
use "collections"
use "buffered"


actor OutputCheck
  """
  A helper for integration tests which can only be checked based on their 
  output (and not eg because they simply don't crash). Assumption: such
  integration tests are each run in their own process.

  The initial process is the test driver and performs asserts on the output.
  When ```run()```, OutputCheck respawns the current executable (nb it doesn't
  fork it), passing in a command line argument. The command line argument is
  a trigger that the child process should run in test mode and produce (or not)
  the output which is expected by the parent. If OutputCheck determines that 
  the output is what is expected, the parent exits with code 0, thereby reporting
  success to ```make```. If is not what is expected, or the child process ends
  with a non-zero error code, a non-zero error code is returned by parent.

  This leads to code like this in the Main actor:
  ```
    if env.args.size() > 1
    then
      // we're the child/test process
      // ADD CODE HERE TO PERFORM THE TEST, 
      // WITH OUTPUT SHOWING TEST PASS / FAIL
    else
      // we're the parent driver/assert process
      let expected = recover val
        [ SET OF STRINGS THAT ARE EXPECTED TO BE OUTPUT ]
      end
      OutputCheck(expected).run(env)
    end
  ```

  Currently verification of the actual and expected output is based on them
  being the same *set* of line strings. This could obviously be extended to
  consider ordering.

  Standard error output from the child process is echoed to the parent process's
  stdout.
  """
  let _expected : Array[String] val

  new create(expectedResults: Array[String] val) =>
    _expected = expectedResults

  be run(env: Env) =>
    let client = _ProcessClient(env, _expected)
    let processFilename: String = try env.args(0) else "" end
    // define the binary to run
    try
      let path = FilePath(env.root as AmbientAuth, 
                          processFilename)

      // define the arguments; first arg is always the binary name
      let args: Array[String] iso = recover [processFilename; "dummy to avoid reforking"] end

      // create a ProcessMonitor and spawn the child process
      let auth = env.root as AmbientAuth
      let pm: ProcessMonitor = ProcessMonitor(auth, 
        consume client, path,
        consume args, recover val Array[String]() end)
    else
      env.out.print("Could not create FilePath!")
    end


class _ProcessClient is ProcessNotify
  let _env: Env
  let _expected: Array[String] val
  let _actual: Reader = Reader

  new iso create(env: Env, expected: Array[String] val) =>
    _env = env
    _expected = expected

  fun ref stdout(process: ProcessMonitor ref, data: Array[U8] iso) =>
    _actual.append(consume data)

  fun ref stderr(process: ProcessMonitor ref, data: Array[U8] iso) =>
    let err = String.from_array(consume data)
    _env.out.print("STDERR: " + err)

  fun ref failed(process: ProcessMonitor ref, err: ProcessError) =>
    match err
    | ExecveError => _env.out.print("ProcessError: ExecveError")
    | PipeError => _env.out.print("ProcessError: PipeError")
    | ForkError => _env.out.print("ProcessError: ForkError")
    | WaitpidError => _env.out.print("ProcessError: WaitpidError")
    | WriteError => _env.out.print("ProcessError: WriteError")
    | KillError => _env.out.print("ProcessError: KillError")
    | CapError => _env.out.print("ProcessError: CapError")
    | Unsupported => _env.out.print("ProcessError: Unsupported")
    else _env.out.print("Unknown ProcessError!")
    end
    _exit(1)

  fun _exit(code: I32) =>
    @_exit[None](I32(code))

  fun ref dispose(process: ProcessMonitor ref, child_exit_code: I32) =>
    if child_exit_code != 0
    then
      _env.out.print("Child exit code: " + child_exit_code.string())
      _exit(child_exit_code)
      return
    end

    let expectedS = Set[String]
    for s in _expected.values() do
      expectedS.set(s)
    end

    let actualS = Set[String]
    while true do
      actualS.set(try _actual.line() else break end)
    end

    if expectedS != actualS
    then
      _exit(1)
    else
      _exit(0)
    end
