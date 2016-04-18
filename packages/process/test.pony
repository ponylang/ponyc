use "ponytest"
use "files"

actor Main is TestList  
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestCat)
    test(_TestStderr)
    test(_TestWc)

class iso _TestCat is UnitTest
  fun name(): String =>
    "writing to STDIN of a forked cat process"

  fun apply(h: TestHelper) =>
    let client = ProcessClient("one, two, three", h)
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
    h.long_test(2_000_000_000)
    pm.dispose()

  fun timed_out(h: TestHelper) =>
    h.complete(false)

class iso _TestStderr is UnitTest
  fun name(): String =>
    "writing to STDERR of via a forked cat process that fails"

  fun apply(h: TestHelper) =>
    let client = ProcessClient("cat: foobaz: No such file or directory\n", h)
    let notifier: ProcessNotify iso = consume client
    let path: String = "/bin/cat"
    
    let args: Array[String] iso = recover Array[String](2) end
    args.push("cat")
    args.push("foobaz")
    
    let vars: Array[String] iso = recover Array[String](2) end
    vars.push("HOME=/")
    vars.push("PATH=/bin")
    
    let pm: ProcessMonitor = ProcessMonitor(consume notifier, path,
      consume args, consume vars)
    pm.write("one, two, three")
    h.long_test(2_000_000_000)
    pm.dispose()

  fun timed_out(h: TestHelper) =>
    h.complete(false)

    
class iso _TestWc is UnitTest
  fun name(): String =>
    "capturing STDOUT and STDERR of forked wc process"

  fun apply(h: TestHelper) =>
    // we expect wc to give a count of
    // 1 line, 9 words, 44 characters, filename foobar
    let client = ProcessClient("       1       9      44 foobar\n", h) 

    let path: String = "/usr/bin/wc"
    
    let args: Array[String] iso = recover Array[String](2) end
    args.push("wc")
    args.push("foobar")
    
    let vars: Array[String] iso = recover Array[String](2) end
    vars.push("HOME=/")
    vars.push("PATH=/bin")
    
    let notifier: ProcessNotify iso = consume client
    let pm: ProcessMonitor = ProcessMonitor(consume notifier, path,
      consume args, consume vars)
    h.long_test(2_000_000_000)
    pm.dispose()

  fun timed_out(h: TestHelper) =>
    h.complete(false)


class ProcessClient is ProcessNotify
  """
  Notifications for Process connections.
  """
  let _e: String
  let _h: TestHelper

  
  new iso create(e: String, h: TestHelper) =>
    _e = e
    _h = h

  fun ref stdout(data: Array[U8] iso) =>
    """
    Called when new data is received on STDOUT of the forked process
    """
    let d = String.from_array(consume data)
    _h.log("in received_stdout: " + d)
    _h.log("should match:       " + _e)
    _h.assert_eq[String](_e, d)

  fun ref stderr(data: Array[U8] iso) =>
    """
    Called when new data is received on STDERR of the forked process
    """
    let d = String.from_array(consume data)
    _h.log("in received_stderr: " + d)
    _h.log("should match:       " + _e)
    _h.assert_eq[String](_e, d)
    
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
    | Unsupported  => _h.fail("ProcessError: Unsupported") 
    else
      _h.fail("Unknown ProcessError!")
    end
    
  fun ref dispose(child_exit_code: I32) =>
    """
    Called when ProcessMonitor terminates to cleanup ProcessNotify
    We return the exit code of the child process.
    """
    _h.log("Child exit code: " + child_exit_code.string())
    _h.complete(true)

  
