# The three ASIO backends implement the signal registration protocol in lockstep

The atomic compare-and-swap signal registration protocol — the `registered`
tri-state, the
subscribe wait/insert/re-verify loop, the cancel claim/re-scan/teardown, and
the seq_cst orderings on the four racing accesses — is implemented separately
in `src/libponyrt/asio/epoll.c`, `kqueue.c`, and `sock_notify.c` (the
backends are platform-exclusive compilation units; only the OS install/
teardown bodies and the delivery channel differ). A concurrency fix applied
to one backend and not the others is invisible on every other platform: the
files compile exclusively, and no shared code enforces the shape.

Two invariants the protocol depends on, in all three copies:

- The four racing accesses (subscriber's slot-insert CAS and state re-verify
  load; canceler's claim CAS and post-claim re-scan loads) must stay seq_cst
  — the access pattern is a store-buffer shape that acquire/release does not
  make safe off x86.
- No systematic-testing yield or parking call may ever be introduced inside
  the claimed (`-1`) install/teardown windows. The subscribe spin-wait has no
  yield point; it is deadlock-free under `use=systematic_testing` only
  because a thread can never be observed mid-install/mid-teardown there. A
  yield inside a claimed window creates a systematic-only deadlock that
  normal CI never sees.

The dispatch-exit teardown is part of the lockstep too: each backend sweeps
its table before freeing the backend, restoring the default disposition for
every still-registered signal, so an undisposed handler's signal can't run
the C handler against a freed backend (#5564). So are the shutdown guards on
the actor-facing entry points: every path that dereferences the backend —
each backend's `send_request`, sock_notify's `sock_notify_ctl`, and the
`PONY_API pony_asio_event_*` functions that touch `b` directly — checks
`ponyint_asio_get_backend()` for NULL first and drops the work, because a
signal delivered during shutdown can wake an actor after quiescence and its
dispose would otherwise dereference the freed backend. Removing a guard in one backend reintroduces
that shutdown crash on that platform only.
`test/full-program-tests/signal-shutdown-disposition` pins the sweep's
disposition restore on every platform.

One more cross-layer contract rides on this protocol: the `arg` value sent
with a signal event's `ASIO_ERROR` encodes the failure reason
(`SIGNAL_ERR_SUBSCRIBER_LIMIT` = 1, `SIGNAL_ERR_REFUSED` = 2, defined in each
backend), and `packages/signals/signal_handler.pony` maps it to
`SignalSubscriberLimit`/`SignalRegistrationRefused`. Changing the codes in a
backend without the stdlib mapping (or vice versa) misreports failure reasons
on that platform only.

Run: `make test-stdlib-debug` and `make test-stdlib-release` on the platform
whose backend changed (Linux = epoll, macOS/BSD = kqueue, Windows =
sock_notify), plus a `use=systematic_testing` build running a trivial program
to the success banner when the epoll dispatch or spin paths change.
