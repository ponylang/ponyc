class val ProcessError
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
  | PipeError
  | ForkError
  | WaitpidError
  | WriteError
  | KillError
  | CapError
  | ChdirError
  | UnknownError
  )

primitive ExecveError
  fun string(): String iso^ => "ExecveError".clone()

primitive PipeError
  fun string(): String iso^ => "PipeError".clone()

primitive ForkError
  fun string(): String iso^ => "ForkError".clone()

primitive WaitpidError
  fun string(): String iso^ => "WaitpidError".clone()

primitive WriteError
  fun string(): String iso^ => "WriteError".clone()

primitive KillError   // Not thrown at this time
  fun string(): String iso^ => "KillError".clone()

primitive CapError
  fun string(): String iso^ => "CapError".clone()

primitive ChdirError
  fun string(): String iso^ => "ChdirError".clone()

primitive UnknownError
  fun string(): String iso^ => "UnknownError".clone()
