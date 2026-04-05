## Add style/docstring-leading-blank lint rule

pony-lint now flags docstrings where a blank line immediately follows the opening `"""`. The first line of content should begin on the line right after the opening delimiter.

```pony
// Flagged — blank line after opening """
class Foo
  """

  Foo docstring.
  """

// Clean — content starts on the next line
class Foo
  """
  Foo docstring.
  """
```

Types and methods annotated with `\nodoc\` are exempt, consistent with `style/docstring-format`.

## Fix rare silent connection hangs on macOS and BSD

On macOS and BSD, if the OS failed to fully register an I/O event (due to resource exhaustion or an FD race), the failure was silently ignored. Actors waiting for network events that were never registered would hang indefinitely with no error. This could appear as connections that never complete, listeners that stop accepting, or timers that stop firing — with no indication of what went wrong.

The runtime now detects these registration failures and notifies the affected actor, which tears down cleanly — the same as any other I/O failure. Stdlib consumers like `TCPConnection` and `TCPListener` handle this automatically.

If you implement `AsioEventNotify` outside the stdlib, you can now detect registration failures with the new `AsioEvent.errored` predicate. Without handling it, a failure is silently ignored (the same behavior as before, but now you have the option to detect it):

```pony
be _event_notify(event: AsioEventID, flags: U32, arg: U32) =>
  if AsioEvent.errored(flags) then
    // Registration failed — tear down
    _close()
    return
  end
  // ... normal event handling
```

## Fix rare silent connection hangs on Linux

On Linux, if the OS failed to register an I/O event (due to resource exhaustion or an FD race), the failure was silently ignored. Actors waiting for network events that were never registered would hang indefinitely with no error. This could appear as connections that never complete, listeners that stop accepting, or timers that stop firing — with no indication of what went wrong.

The runtime now detects these registration failures and notifies the affected actor, which tears down cleanly — the same as any other I/O failure. Stdlib consumers like `TCPConnection` and `TCPListener` handle this automatically.

Also fixes the ASIO backend init to correctly detect `epoll_create1` and `eventfd` failures (previously checked for 0 instead of -1), and to clean up all resources on partial init failure.

## LSP: Fix goto_definition range end

The Pony language server `textDocument/definition` response now returns a correct `range.end` position. Previously, it had an off-by-one error in the column value.

