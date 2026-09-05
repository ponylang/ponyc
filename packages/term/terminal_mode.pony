use @pony_os_tty_set_raw[Bool]()
use @pony_os_tty_restore[None]()

primitive TerminalMode
  """
  Controls the terminal mode for stdin.

  In raw mode, input characters are delivered one at a time without echo
  or line editing. ISIG remains on, so Ctrl-C still generates SIGINT.
  """
  fun set_raw(auth: TerminalAuth): Bool =>
    """
    Put stdin into pseudo-raw mode. Returns true if the terminal was
    changed, false if stdin is not a terminal or the call failed.
    """
    @pony_os_tty_set_raw()

  fun restore(auth: TerminalAuth) =>
    """
    Restore stdin to its original terminal mode.
    """
    @pony_os_tty_restore()
