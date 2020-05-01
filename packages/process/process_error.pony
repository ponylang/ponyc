class val ProcessError
  let error_type: ProcessErrorType
  let message: (String | None)

  new val create(error_type': ProcessErrorType,
    message': (String | None) = None)
  =>
    error_type = error_type'
    message = message'

  fun string(): String =>
    match message
    | let m: String => error_type.string() + ": " + m
    else error_type.string()
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
  fun string(): String => "ExecveError"

primitive PipeError
  fun string(): String => "PipeError"

primitive ForkError
  fun string(): String => "ForkError"

primitive WaitpidError
  fun string(): String => "WaitpidError"

primitive WriteError
  fun string(): String => "WriteError"

primitive KillError   // Not thrown at this time
  fun string(): String => "KillError"

primitive CapError
  fun string(): String => "CapError"

primitive ChdirError
  fun string(): String => "ChdirError"

primitive UnknownError
  fun string(): String => "UnknownError"
