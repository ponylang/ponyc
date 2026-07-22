class val ProcessError
  """
  Why a process could not be started, or why monitoring one failed.
  `StartProcess` returns a `ProcessError` when it cannot start a process, and
  `ProcessNotify.failed` receives one for errors that arise while a process
  runs. `error_type` is the category of the error; `message` is optional
  human-readable detail.
  """
  let error_type: ProcessErrorType
  let message: (String | None)

  new val create(error_type': ProcessErrorType,
    message': (String | None) = None)
  =>
    error_type = error_type'
    message = message'

  fun string(): String iso^ =>
    match message
    | let m: String =>
      recover
        let etc = error_type.string()
        let err = String(etc.size() + 2 + m.size())
        err.append(consume etc)
        err.append(": ")
        err.append(m)
        err
      end
    else
      error_type.string()
    end

type ProcessErrorType is
  ( ExecveError
  | ExecutableNotFound
  | UnsupportedKernel
  | PipeError
  | ForkError
  | WaitpidError
  | WriteError
  | KillError
  | CapError
  | ChdirError
  | UnknownError
  )
  """
  The category of a `ProcessError`.
  """

primitive ExecveError
  """
  `execve` failed in the child after the process was started, so the program
  never ran. Arrives through `ProcessNotify.failed`. For a path that does not
  name a runnable file in the first place, see `ExecutableNotFound`.
  """
  fun string(): String iso^ => "ExecveError".clone()

primitive ExecutableNotFound
  """
  The path given to start a process does not name an existing file we can try
  to execute; it is missing or is a directory. Distinct from `ExecveError`,
  which is `execve` failing in the child after the process was started.
  """
  fun string(): String iso^ => "ExecutableNotFound".clone()

primitive UnsupportedKernel
  """
  The platform cannot report a child's exit. On Linux this means the kernel is
  older than 5.3 and lacks `pidfd_open`.
  """
  fun string(): String iso^ => "UnsupportedKernel".clone()

primitive PipeError
  """
  A pipe to communicate with the child could not be opened. Returned by
  `StartProcess`.
  """
  fun string(): String iso^ => "PipeError".clone()

primitive ForkError
  """
  The child process could not be created (`fork` on posix, `CreateProcess` on
  Windows). Returned by `StartProcess`.
  """
  fun string(): String iso^ => "ForkError".clone()

primitive WaitpidError
  """
  The child's exit status could not be read. Arrives through
  `ProcessNotify.failed` as a terminal error in place of `dispose`.
  """
  fun string(): String iso^ => "WaitpidError".clone()

primitive WriteError
  """
  Writing to the child's STDIN failed. Arrives through `ProcessNotify.failed`.
  """
  fun string(): String iso^ => "WriteError".clone()

primitive KillError
  """
  Currently unused.
  """
  fun string(): String iso^ => "KillError".clone()

primitive CapError
  """
  The path does not have the `FileExec` capability, so we are not permitted to
  execute it. Returned by `StartProcess`.
  """
  fun string(): String iso^ => "CapError".clone()

primitive ChdirError
  """
  The child could not change to the requested working directory. Arrives
  through `ProcessNotify.failed`.
  """
  fun string(): String iso^ => "ChdirError".clone()

primitive UnknownError
  """
  The child reported a startup failure we could not classify. Arrives through
  `ProcessNotify.failed`.
  """
  fun string(): String iso^ => "UnknownError".clone()
