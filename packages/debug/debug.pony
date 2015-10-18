primitive Debug
  """
  This is a debug only print utility.
  """
  fun out(msg: String = "") =>
    """
    If platform is debug configured, print message to standard output
    """
    let stream = @os_stdout[Pointer[U8]]()
    _print(stream, msg + "\n")

  fun err(msg: String = "") =>
    """
    If platform is debug configured, print message to standard error
    """
    let stream = @os_stderr[Pointer[U8]]()
    _print(stream, msg + "\n")

fun _print(stream: Pointer[U8], msg: String) =>
    if Platform.debug() then
      if msg.size() > 0 then
        @fwrite[U64]( msg.cstring(), U64(1), msg.size(), stream )
      end
    end
