# The 16-per-signal subscriber cap is a cross-layer contract

`MAX_SIGNAL_SUBSCRIBERS` (16) is defined independently in each ASIO backend —
`src/libponyrt/asio/epoll.c`, `src/libponyrt/asio/kqueue.c`,
`src/libponyrt/asio/sock_notify.c` — and is documented user-facing behavior in
the `signals` stdlib package: the `SignalHandler` and package docstrings state
the cap and the auto-dispose-on-error behavior, the release notes state it,
and `packages/signals/_test.pony` asserts it (the subscriber-limit test
registers 16 handlers and requires the 17th to error-dispose). Changing the
cap in one backend desynchronizes the platforms; changing it in all three
still breaks the stdlib test and silently falsifies the docstrings and any
published release notes. A backend-only change won't be caught by test-core —
the assertion lives in the stdlib suite.

Run: `make test-stdlib-debug` and `make test-stdlib-release` (Linux covers
epoll; Windows CI covers sock_notify; kqueue is covered by the macOS/BSD CI
legs).
