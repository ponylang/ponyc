use "process"

class iso _BuildProcessNotify is ProcessNotify
  let _tester: _Tester tag
  var _done: Bool

  new iso create(tester: _Tester tag) =>
    _tester = tester
    _done = false

  fun ref stdout(p: ProcessMonitor ref, data: Array[U8] iso) =>
    _tester.append_stdout(consume data)

  fun ref stderr(p: ProcessMonitor ref, data: Array[U8] iso) =>
    _tester.append_stderr(consume data)

  fun ref failed(p: ProcessMonitor ref, pe: ProcessError) =>
    if not _done then
      _done = true
      let msg =
        match pe.message
        | let msg': String => msg'
        else "build process failed to start"
        end
      _tester.building_failed(msg)
    end

  fun ref dispose(p: ProcessMonitor, status: ProcessExitStatus) =>
    if not _done then
      _done = true
      match \exhaustive\ status
      | let exited: Exited =>
        if exited.exit_code() == 0 then
          _tester.building_succeeded()
        else
          _tester.building_failed("exited with code "
            + exited.exit_code().string())
        end
      | let signaled: Signaled =>
        _tester.building_failed("signal " + signaled.signal().string())
      end
    end
