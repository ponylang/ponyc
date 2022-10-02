use "pony_test"
use "backpressure"
use "capsicum"
use "collections"
use "files"
use "itertools"
use "time"
use "signals"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestBadChdir)
    test(_TestBadExec)
    test(_TestChdir)

class \nodoc\ _TestBadChdir is UnitTest
  fun name(): String =>
    "process/BadChdir"

  fun exclusion_group(): String =>
    "process-monitor"

  fun ref apply(h: TestHelper) =>
    let badpath = Path.abs(Path.random(10))
    let notifier: ProcessNotify iso = _NoStartNotify(h)

    let process_auth = StartProcessAuth(h.env.root)
    let file_auth = FileAuth(h.env.root)

    let path = FilePath(file_auth, _PwdPath())
    let args: Array[String] val = _PwdArgs()
    let vars: Array[String] iso = recover Array[String](0) end

    try
      ProcessMonitorCreator(
        process_auth,
        consume notifier,
        path,
        args,
        consume vars,
        FilePath(file_auth, badpath))?

      h.fail("process monitor creation didn't fail")
    else
      // test passed
      None
    end

class \nodoc\ _TestBadExec is UnitTest
  let _bad_exec_sh_contents: String =
    """
    #!/usr/bin/please-dont-exist

    true
    """
  var _tmp_dir: ( FilePath | None ) = None
  var _bad_exec_path: ( FilePath | None ) = None

  fun name(): String =>
    "process/BadExec"

  fun exclusion_group(): String =>
    "process-monitor"

  fun ref set_up(h: TestHelper) ? =>
    let file_auth = FileAuth(h.env.root)

    ifdef windows then
      _bad_exec_path = FilePath(file_auth, "C:\\Windows\\system.ini")
    else
      let tmp_dir = FilePath.mkdtemp(file_auth,
        "pony_stdlib_test_process_bad_exec")?
      _tmp_dir = tmp_dir
      let bad_exec_path = tmp_dir.join("_bad_exec.sh")?
      _bad_exec_path = bad_exec_path

      // create file with non-existing shebang in temporary directory
      let file = File(bad_exec_path)
      file.write(_bad_exec_sh_contents)
      if not file.chmod(FileMode.>exec()) then error end
    end

  fun ref tear_down(h: TestHelper) =>
    try
      (_tmp_dir as FilePath).remove()
    end

  fun ref apply(h: TestHelper) =>
    let notifier = _NoStartNotify(h)
    let process_auth = StartProcessAuth(h.env.root)

    try
      let path = try
        _bad_exec_path as FilePath
      else
        h.fail("bad_exec_path not set!")
        return
      end

      ProcessMonitorCreator(
        process_auth,
        consume notifier,
        path,
        [],
        [])?

      h.fail("process monitor creation didn't fail")
    else
      // test passed
      None
    end

class \nodoc\ _TestChdir is UnitTest
  fun name(): String =>
    "process/Chdir"

  fun exclusion_group(): String =>
    "process-monitor"

  fun ref apply(h: TestHelper) =>
    let process_auth = StartProcessAuth(h.env.root)
    let file_auth = FileAuth(h.env.root)

    let parent = Path.dir(Path.cwd())
    // expect path length + \n
    let notifier: ProcessNotify iso = _ProcessClient(parent.size()
      + (ifdef windows then 2 else 1 end), "", 0, h)

    let path = FilePath(file_auth, _PwdPath())
    let args: Array[String] val = _PwdArgs()
    let vars: Array[String] val = ["HOME=/"; "PATH=/bin"]

    try
      let pm = ProcessMonitorCreator(
        process_auth,
        consume notifier,
        path,
        args,
        vars,
        FilePath(file_auth, parent))?

      pm.done_writing()
      h.dispose_when_done(pm)
      h.long_test(30_000_000_000)
    else
      h.fail("unable to create process monitor")
    end

class \nodoc\ _NoStartNotify is ProcessNotify
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref stdout(process: ProcessMonitor ref, data: Array[U8] iso) =>
    _h.fail("stdout shouldn't be called")

  fun ref stderr(process: ProcessMonitor ref, data: Array[U8] iso) =>
    _h.fail("stderr shouldn't be called")

   fun ref failed(process: ProcessMonitor ref, actual: ProcessError) =>
     _h.fail("failed shouldn't be called")

  fun ref dispose(
    process: ProcessMonitor ref,
    child_exit_status: ProcessExitStatus)
  =>
    _h.fail("dispose shouldn't be called")

class \nodoc\ _ProcessClient is ProcessNotify
  """
  Notifications for Process connections.
  """
  let _out: USize
  let _err: String
  let _exit_code: I32
  let _h: TestHelper
  var _d_stdout_chars: USize = 0
  let _d_stderr: String ref = String
  let _created: U64
  var _first_data: U64 = 0
  let _expected_err: (ProcessError | None)

  new iso create(
    out: USize,
    err: String,
    exit_code: I32,
    h: TestHelper,
    expected_err: (ProcessError | None) = None)
  =>
    _out = out
    _err = err
    _exit_code = exit_code
    _expected_err = expected_err
    _h = h
    _created = Time.nanos()

  fun ref stdout(process: ProcessMonitor ref, data: Array[U8] iso) =>
    """
    Called when new data is received on STDOUT of the forked process
    """
    _h.log("\tReceived from stdout: " + data.size().string() + " bytes")
    if (_first_data == 0) then
      _first_data = Time.nanos()
    end
    _d_stdout_chars = _d_stdout_chars + data.size()
    _h.log("\tReceived so far: " + _d_stdout_chars.string() + " bytes")
    _h.log("\tExpecting: " + _out.string() + " bytes")
    if _out == _d_stdout_chars then
      // we've received our total data
      _h.complete(true)
    end

  fun ref stderr(process: ProcessMonitor ref, data: Array[U8] iso) =>
    """
    Called when new data is received on STDERR of the forked process
    """
    _h.log("\tReceived from stderr: " + data.size().string() + " bytes")
    _d_stderr.append(consume data)

  fun ref failed(process: ProcessMonitor ref, actual: ProcessError) =>
    """
    ProcessMonitor calls this if we run into errors with the
    forked process.
    """
    match _expected_err
    | let expected: ProcessError =>
      if actual.error_type is expected.error_type then
        _h.complete(true)
      else
        _h.fail("Expected '" + expected.string() + "'; actual was '"
          + actual.string() + "'")
      end
    else
      _h.fail(actual.string())
    end

  fun ref dispose(process: ProcessMonitor ref, child_exit_status: ProcessExitStatus) =>
    """
    Called when ProcessMonitor terminates to cleanup ProcessNotify
    We receive the exit code of the child process from ProcessMonitor.
    """
    let last_data: U64 = Time.nanos()
    match child_exit_status
    | let e: Exited =>
      _h.log("dispose: child exit code: " + e.exit_code().string())
      _h.assert_eq[I32](_exit_code, e.exit_code())
    | let s: Signaled =>
      _h.log("dispose: child terminated by signal: " + s.signal().string())
      _h.fail("didn't expect that.")
    end
    _h.log("dispose: stdout: " + _d_stdout_chars.string() + " bytes")
    _h.log("dispose: stderr: '" + _d_stderr + "'")
    if (_first_data > 0) then
      _h.log("dispose: received first data after: \t"
        + (_first_data - _created).string() + " ns")
    end
    _h.log("dispose: total data process_time: \t"
      + (last_data - _first_data).string() + " ns")
    _h.log("dispose: ProcessNotify lifetime: \t"
      + (last_data - _created).string() + " ns")

    _h.assert_eq[USize](_out, _d_stdout_chars)
    _h.assert_eq[USize](_err.size(), _d_stderr.size())
    _h.assert_eq[String box](_err, _d_stderr)
    _h.complete(true)

primitive \nodoc\ _PwdPath
  fun apply(): String =>
    ifdef windows then
      "C:\\Windows\\System32\\cmd.exe" // Have to use "cmd /c cd"
    else
      "/bin/pwd"
    end

primitive \nodoc\ _PwdArgs
  fun apply(): Array[String] val =>
    ifdef windows then
      ["cmd"; "/c"; "cd"]
    else
      ["pwd"]
    end
