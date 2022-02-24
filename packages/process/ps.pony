use "files"

trait _ProcessMonitorStateHandler

class _StartupHandler is _ProcessMonitorStateHandler
  var _notifier: ProcessNotify2 iso

  new create(notifier: ProcessNotify2 iso) =>
    _notifier = consume notifier

  fun ref start(filepath: FilePath):
    (_ProcessFailedToStart | _ProcessStartingUp)
  =>
    if not filepath.caps(FileExec) then
      let e = ProcessError(CapError,
        filepath.path
        + " is not an executable or we do not have execute capability.")
      return _fail(e)
    end

    _ProcessStartingUp

  fun ref _fail(e: ProcessError): _ProcessFailedToStart =>
    let n = _notifier = ConsumedProcessNotify2
    _ProcessFailedToStart(consume n, e)

class _ProcessFailedToStart is _ProcessMonitorStateHandler
  let _notifier: ProcessNotify2 iso

  new create(notifier: ProcessNotify2 iso, pe: ProcessError) =>
    _notifier = consume notifier
    _notifier.failed(this, pe)

class _ProcessNotStarted is _ProcessMonitorStateHandler

class _ProcessRunning is _ProcessMonitorStateHandler

class _ProcessStartingUp is _ProcessMonitorStateHandler

class _ProcessFinished is _ProcessMonitorStateHandler
