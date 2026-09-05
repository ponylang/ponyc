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
