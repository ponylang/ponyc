type ProcessError is
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

class val ExecveError
  let _message: String

  new val create(message: String) =>
    _message = message

  fun string(): String => _message

class val PipeError
  let _message: String

  new val create(message: String) =>
    _message = message

  fun string(): String => _message

class val ForkError
  let _message: String

  new val create(message: String) =>
    _message = message

  fun string(): String => _message

class val WaitpidError
  let _message: String

  new val create(message: String) =>
    _message = message

  fun string(): String => _message

class val WriteError
  let _message: String

  new val create(message: String) =>
    _message = message

  fun string(): String => _message

class val KillError   // Not thrown at this time
  let _message: String

  new val create(message: String) =>
    _message = message

  fun string(): String => _message

class val CapError
  let _message: String

  new val create(message: String) =>
    _message = message

  fun string(): String => _message

class val ChdirError
  let _message: String

  new val create(message: String) =>
    _message = message

  fun string(): String => _message

class val UnknownError
  fun string(): String => "Unknown error."
