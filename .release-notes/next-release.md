## Make terminal raw mode opt-in and auth-gated

The runtime no longer puts stdin into pseudo-raw mode at startup. Programs that read from `env.input` without using `ANSITerm` now get normal cooked-mode input: echoed, line-buffered, with Ctrl-D interpreted as EOF.

`ANSITerm` now requires a `TerminalAuth` (derived from `AmbientAuth`) and sets raw mode itself on construction. It also restores the original terminal mode on dispose and re-applies raw mode after a suspend/resume (SIGCONT).

Before:

```pony
use "signals"
use "term"

actor Main
  new create(env: Env) =>
    let term = ANSITerm(SignalAuth(env.root), notify, env.input)
```

After:

```pony
use "signals"
use "term"

actor Main
  new create(env: Env) =>
    let term = ANSITerm(
      SignalAuth(env.root), TerminalAuth(env.root), notify, env.input)
```

For programs that need raw mode without `ANSITerm`, use `TerminalMode` directly:

```pony
use "term"

actor Main
  new create(env: Env) =>
    let auth = TerminalAuth(env.root)
    TerminalMode.set_raw(auth)
    // ... read raw input ...
    TerminalMode.restore(auth)
```

## Fix iftype capability narrowing inside generic return types

When a method on a generic class used `iftype` to narrow a type parameter's capability and returned a generic container parameterized by that type parameter, the compiler rejected the body with "function body isn't the result type."

For example, a method returning `MyBox[A]^` that produces `recover iso MyBox[A].create() end` inside an `iftype A <: Any val` branch was rejected.

This now compiles correctly. The fix applies to all capability constraints, including `#share`.

