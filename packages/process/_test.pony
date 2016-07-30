use "ponytest"
use "files"
use "capsicum"
use "collections"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestStdinStdout)
    test(_TestStderr)
    test(_TestFileExec)

class iso _TestStdinStdout is UnitTest
  fun name(): String =>
    "process/STDIN-STDOUT"

  fun apply(h: TestHelper) =>
    let notifier: ProcessNotify iso = _ProcessClient("one, two, three",
      "", 0, h)
    try
      let path = FilePath(h.env.root as AmbientAuth, "/bin/cat")
      let args: Array[String] iso = recover Array[String](1) end
      args.push("cat")
      let vars: Array[String] iso = recover Array[String](2) end
      vars.push("HOME=/")
      vars.push("PATH=/bin")

      let pm: ProcessMonitor = ProcessMonitor(consume notifier, path,
        consume args, consume vars)
        pm.write("one, two, three")
        pm.done_writing()  // closing stdin allows "cat" to terminate
        h.long_test(5_000_000_000)
    else
      h.fail("Could not create FilePath!")
    end

  fun timed_out(h: TestHelper) =>
    h.complete(false)

class iso _TestStderr is UnitTest
  fun name(): String =>
    "process/STDERR"

  fun apply(h: TestHelper) =>
    let notifier: ProcessNotify iso = _ProcessClient("",
      "cat: file_does_not_exist: No such file or directory\n", 1, h)
    try
      let path = FilePath(h.env.root as AmbientAuth, "/bin/cat")
      let args: Array[String] iso = recover Array[String](2) end
      args.push("cat")
      args.push("file_does_not_exist")

      let vars: Array[String] iso = recover Array[String](2) end
      vars.push("HOME=/")
      vars.push("PATH=/bin")

      let pm: ProcessMonitor = ProcessMonitor(consume notifier, path,
        consume args, consume vars)
        h.long_test(5_000_000_000)
    else
      h.fail("Could not create FilePath!")
    end

  fun timed_out(h: TestHelper) =>
    h.complete(false)

class iso _TestFileExec is UnitTest
  fun name(): String =>
    "process/FileExec"

  fun apply(h: TestHelper) =>
    let notifier: ProcessNotify iso = _ProcessClient("",
      "cat: file_does_not_exist: No such file or directory\n", 1, h)
    try
      let path = FilePath(h.env.root as AmbientAuth, "/bin/date",
        recover val FileCaps.all().unset(FileExec) end)
      let args: Array[String] iso = recover Array[String](1) end
      args.push("date")
      let vars: Array[String] iso = recover Array[String](2) end
      vars.push("HOME=/")
      vars.push("PATH=/bin")

      let pm: ProcessMonitor = ProcessMonitor(consume notifier, path,
        consume args, consume vars)
      h.long_test(5_000_000_000)
    else
      h.fail("Could not create FilePath!")
    end

  fun timed_out(h: TestHelper) =>
    h.complete(false)

class _ProcessClient is ProcessNotify
  """
  Notifications for Process connections.
  """
  let _out: String
  let _err: String
  let _exit_code: I32
  let _h: TestHelper
  let _d_stdout: String ref = String
  let _d_stderr: String ref = String

  new iso create(out: String, err: String, exit_code: I32,
    h: TestHelper) =>
    _out = out
    _err = err
    _exit_code = exit_code
    _h = h

  fun ref stdout(process: ProcessMonitor ref, data: Array[U8] iso) =>
    """
    Called when new data is received on STDOUT of the forked process
    """
    _d_stdout.append(consume data)

  fun ref stderr(process: ProcessMonitor ref, data: Array[U8] iso) =>
    """
    Called when new data is received on STDERR of the forked process
    """
    _d_stderr.append(consume data)

  fun ref failed(process: ProcessMonitor ref, err: ProcessError) =>
    """
    ProcessMonitor calls this if we run into errors with the
    forked process.
    """
    match err
    | ExecveError   => _h.fail("ProcessError: ExecveError")
    | PipeError     => _h.fail("ProcessError: PipeError")
    | Dup2Error     => _h.fail("ProcessError: Dup2Error")
    | ForkError     => _h.fail("ProcessError: ForkError")
    | FcntlError    => _h.fail("ProcessError: FcntlError")
    | WaitpidError  => _h.fail("ProcessError: WaitpidError")
    | CloseError    => _h.fail("ProcessError: CloseError")
    | ReadError     => _h.fail("ProcessError: ReadError")
    | WriteError    => _h.fail("ProcessError: WriteError")
    | KillError     => _h.fail("ProcessError: KillError")
    | Unsupported   => _h.fail("ProcessError: Unsupported")
    | CapError      => _h.complete(true) // used in _TestFileExec
    else
      _h.fail("Unknown ProcessError!")
    end

  fun ref dispose(process: ProcessMonitor ref, child_exit_code: I32) =>
    """
    Called when ProcessMonitor terminates to cleanup ProcessNotify
    We receive the exit code of the child process from ProcessMonitor.
    """
    _h.log("dispose: child exit code: " + child_exit_code.string())
    _h.log("dispose: stdout: " + _d_stdout)
    _h.log("dispose: stderr: " + _d_stderr)
    _h.assert_eq[String box](_out, _d_stdout)
    _h.assert_eq[String box](_err, _d_stderr)
    _h.assert_eq[I32](_exit_code, child_exit_code)
    _h.complete(true)
