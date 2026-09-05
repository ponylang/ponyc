"""
# Term package

The Term package provides support for building text-based user
interfaces in ANSI terminals.

Terminal raw mode is opt-in: programs that use `Stdin` directly get
normal cooked-mode input. To enable raw mode for keystroke-at-a-time
input, construct an `ANSITerm` with a `TerminalAuth` (derived from
`AmbientAuth`). Raw mode is restored automatically after a
suspend/resume (SIGCONT) and reverted to the original mode when the
`ANSITerm` is disposed.

For programs that need raw mode without `ANSITerm`, use
`TerminalMode.set_raw()` and `TerminalMode.restore()` directly.
"""
