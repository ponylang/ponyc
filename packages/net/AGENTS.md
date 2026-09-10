# Net package

The `net` package provides Pony's networking. It reworks the original stdlib `net` package around a different split: the connection logic lives in a plain `class` (`TCPConnection`, `TCPListener`, `UDPSocket`) that the user's `actor` holds and delegates to, rather than being baked into a single actor.

<!-- contributor-only -->
## Contributing with an AI assistant

This is a Pony project. The ponylang org maintains a set of LLM coding skills. Get set up with them before contributing:

- **Not set up yet?** Install them once:

  ```bash
  git clone https://github.com/ponylang/llm-skills.git
  cd llm-skills
  python install.py
  ```

- **Already set up?** Make sure you're on the latest. If you installed with the script above, `git pull` in the directory where you cloned `llm-skills` and the symlinked skills update automatically — if you set them up another way, refresh them however that setup expects.

See the [llm-skills README](https://github.com/ponylang/llm-skills) for details and other harnesses.

When you start working on this project, load the `pony-skills` skill — it tells your assistant which Pony skill to use for each task.

Read [CONTRIBUTING.md](../../CONTRIBUTING.md).
<!-- /contributor-only -->

## Architecture

A client connect subscribes one socket per resolved address at once (Happy Eyeballs). Whichever connects first becomes the connection's own event; the rest are stragglers, and each has to be unsubscribed and its fd closed when its event arrives. That is why every state handles `own_event` and `foreign_event` separately, and what `_UnconnectedClosing` drains.

### Connection lifecycle

`TCPConnection` tracks its lifecycle with explicit state objects — the `_ConnectionState` trait and its implementers in `_connection_state.pony` — rather than boolean flags.

```
_ConnectionNone     → _ClientConnecting
                    → _Open              (server, plaintext)
                    → _SSLHandshaking    (server, SSL)
                    → _Closed            (SSL session creation failed)
_ClientConnecting   → _Open              (connected, plaintext)
                    → _SSLHandshaking    (connected, SSL)
                    → _UnconnectedClosing (close, drain stragglers)
                    → _Closed            (hard_close / all attempts failed)
_SSLHandshaking     → _Open              (ssl_handshake_complete)
                    → _Closed            (hard_close / SSL error)
_Open               → _Closing           (close)
                    → _TLSUpgrading      (start_tls)
                    → _Closed            (hard_close)
_TLSUpgrading       → _Open              (ssl_handshake_complete)
                    → _Closed            (hard_close / TLS error)
_Closing            → _Closed            (drained, or hard_close)
_UnconnectedClosing → _Closed            (drained, or hard_close)
```

### UDP lifecycle

`UDPSocket` uses the same state-object pattern with three states.

```
_UDPNone    → _UDPOpen   (bind succeeded)
            → _UDPClosed (bind failed, or dispose/init race)
_UDPOpen    → _UDPClosed (close or error)
```

The dispose/init race: `_finish_initialization` is a self→self behavior. If `dispose()` arrives first, `_UDPNone.close()` transitions to `_UDPClosed`, and `_finish_initialization` checks for that and skips the bind.

## Traps

- **One read loop; keep `_on_received` out of `_ssl_poll()`.** An earlier design had `_ssl_poll()` deliver application data from a second loop of its own, so mute and the liveness check had to be written twice. Both shipped as bugs before the second copy existed.

- **`_ssl` is a `_TLSState`, not `(SSL | None)`.** `(SSL | None)` was the earlier design. It records whether a session exists but not whether one may be used, and that ambiguity was a segfault.

- **Mint the send token before the flush.** The flush can close the connection, so a token minted after it loses the terminal callback for a send whose bytes already went out. It is also why `send()` returns `SendAccepted` rather than the token: `_on_sent` is a direct call, so the application has to be holding the token before the flush runs. Keep every error return upstream of the mint, so an errored send burns no token id.

- **`_on_send_failed` is deferred; `_on_sent` is not.** `_on_sent` was deferred too, for the same-looking reason, and it was wrong: a send reaching the OS is its own event, and queueing it let callbacks for later events overtake it.

- **The hard-close reason is the `_HardCloseCause` argument, not a field.** An earlier design set one of three fields (`_connect_timed_out` and the like) right before an argumentless `hard_close()` and dug it back out after. Do not put it back in a field.

- **`close_notify` is deferred to `_Closing.drained()`, not sent in `_Open.close()`.** `ssl.close()` calls `SSL_shutdown`, which makes `SSL_read` return `SSL_ERROR_ZERO_RETURN` — buffered TLS records the read loop has not yet delivered are lost.

- **`_set_unwriteable()` before `PonyAsio.resubscribe_write()`.** The ponyc epoll backend's `pony_asio_event_resubscribe()` only includes `EPOLLOUT` when `!ev->writeable`. After `_dispatch_io_event()` sets writeable, the flag stays true, so a later resubscribe is a no-op for the write side. Clear the flag first.

- **A stale foreign event is dropped once, in `_event_notify`, not in each state.** The check used to sit in each `foreign_event` instead. Three of those copies were removed because the suite still passed on Linux, where the second message does not arrive, and that shipped as a bug reachable only where kqueue is the backend. Do not push the check back down into the states.

## Platform differences

POSIX and Windows share one readiness-based I/O path: one-shot readiness events (epoll/kqueue; `ProcessSocketNotifications` on Windows), resubscribe, then a synchronous `RuntimeBackend.receive`/`sendv`. Windows uses this path because ponyc removed IOCP; the floor is Windows 11 / Windows Server 2022.

How many messages one subscription delivers is platform-specific too. kqueue arms read and write as separate one-shot filters and sends a message from each (ponyc's `kqueue.c`), so a single subscribed socket can deliver two readiness messages; epoll and Windows combine both directions into one (`epoll.c`, `sock_notify.c`). Code written on the assumption of one message per subscription is wrong wherever kqueue is the backend — macOS is the one CI covers, but the BSDs use it too.

## Conventions

- `_Unreachable()` in a branch the compiler cannot prove impossible, rather than an empty `else`.
- The runtime will not exit while an actor holds a live I/O resource, so a test has to dispose any actor that might still hold one when it ends, or CI hangs. That is the client a listener creates in `_on_listening` and every server actor returned from `_on_accept`. Happy Eyeballs can deliver more than one connection per client connect (one per resolved address), so store accepted servers in `let _servers: Array[Type]`, push in `_on_accept`, and iterate in `dispose`/`_on_closed`.
- Each test uses its own hardcoded port. Grep `packages/net/_test_*.pony` for a free one.
- `\nodoc\` on test classes.
- A new test goes in the `_test_*.pony` file for its functional area, registered in `Main.tests()` in `_test.pony`, which holds only the test runner.
- Each example has a file-level docstring saying what it demonstrates, uses the Listener/Server/Client actor structure, and uses a unique port. Adding one means adding it to `examples/README.md`, which is ordered simplest first.
