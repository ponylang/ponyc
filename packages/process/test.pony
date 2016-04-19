use "ponytest"
use "files"

actor Main is TestList  
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestStdinStdout)
    test(_TestStderr)

class iso _TestStdinStdout is UnitTest
  fun name(): String =>
    "process/writing to STDIN of a forked cat process capturing STDOUT"

  fun apply(h: TestHelper) =>
    let client = _ProcessClient("one, two, three", "", 0, h)
    let notifier: ProcessNotify iso = consume client
    let path: String = "/bin/cat"
    
    let args: Array[String] iso = recover Array[String](1) end
    args.push("cat")
    
    let vars: Array[String] iso = recover Array[String](2) end
    vars.push("HOME=/")
    vars.push("PATH=/bin")
    
    let pm: ProcessMonitor = ProcessMonitor(consume notifier, path,
      consume args, consume vars)
    pm.write("one, two, three")
    pm.dispose()
    h.long_test(60_000_000_000)

  fun timed_out(h: TestHelper) =>
    h.complete(false)

class iso _TestStderr is UnitTest
  fun name(): String =>
    "process/writing to STDERR of via a forked cat process"

  fun apply(h: TestHelper) =>
    let client = _ProcessClient("",
      "cat: file_does_not_exist: No such file or directory\n", 1, h)
    let notifier: ProcessNotify iso = consume client
    let path: String = "/bin/cat"
    
    let args: Array[String] iso = recover Array[String](2) end
    args.push("cat")
    args.push("file_does_not_exist")
    
    let vars: Array[String] iso = recover Array[String](2) end
    vars.push("HOME=/")
    vars.push("PATH=/bin")
    
    let pm: ProcessMonitor = ProcessMonitor(consume notifier, path,
      consume args, consume vars)
    pm.dispose()
    h.long_test(60_000_000_000)

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
  var _d_stdout: String = ""
  var _d_stderr: String = ""
  
  new iso create(out: String, err: String, exit_code: I32,
    h: TestHelper) =>
    _out = out
    _err = err
    _exit_code = exit_code
    _h = h
    
  fun ref stdout(data: Array[U8] iso) =>
    """
    Called when new data is received on STDOUT of the forked process
    """
    _d_stdout = _d_stdout + String.from_array(consume data)
    _h.log("in received_stdout: " + _d_stdout)
    _h.log("should match:       " + _out)
    if _d_stdout.size() == _out.size() then
      _h.assert_eq[String](_out, _d_stdout)
    end
    
  fun ref stderr(data: Array[U8] iso) =>
    """
    Called when new data is received on STDERR of the forked process
    """
    _d_stderr = _d_stderr + String.from_array(consume data)
    _h.log("in received_stderr: '" + _d_stderr + "'")
    _h.log("should match:       '" + _err + "'")
    if _d_stderr.size() == _err.size() then
      _h.assert_eq[String](_err, _d_stderr)
    end
    
  fun ref failed(err: ProcessError) =>
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
    | Unsupported   => _h.fail("ProcessError: Unsupported") 
    else
      _h.fail("Unknown ProcessError!")
    end
    
  fun ref dispose(child_exit_code: I32) =>
    """
    Called when ProcessMonitor terminates to cleanup ProcessNotify
    We return the exit code of the child process.
    """
    let code: I32 = consume child_exit_code
    _h.log("Child exit code: " + code.string())
    _h.assert_eq[I32](_exit_code, code)
    _h.complete(true)
