# TCPConnection's read re-arm depends on epoll/Windows sharing one read+write registration

The epoll and Windows readiness backends (`src/libponyrt/asio/epoll.c`,
`sock_notify.c`) register a socket for read and write on ONE one-shot registration
(`EPOLLONESHOT`; `SOCK_NOTIFY_TRIGGER_ONESHOT`). One-shot disables the WHOLE
registration after ONE delivered event, so delivering a *writeable* event disarms
the *read* arm too (and vice versa). `pony_asio_event_resubscribe` re-arms both
directions together (IN if `!readable`, OUT if `!writeable`). kqueue
(`kqueue.c`) is DIFFERENT: it registers `EVFILT_READ` and `EVFILT_WRITE` as two
independent one-shot kevents, so a write event disarms only the write filter and
the read arm survives -- the bug below cannot occur there.

`packages/net/tcp_connection.pony` relies on the epoll/Windows shared-registration
model. Its `_pending_reads` (read would-block) and `_apply_backpressure` (write
would-block) re-arm on would-block, which normally re-arms both arms. But a
*writeable* event that drains fully (no backpressure) returns through neither, so
it never re-arms the read arm the writeable delivery just consumed. Without a fix,
a later readable is never delivered and the connection hangs with the runtime idle
-- observed on Windows, where the slow loopback makes read and write readiness
arrive at separate times so the write event consumes the read arm (latent on
epoll, where fast loopback usually delivers them together; impossible on kqueue).
So `_event_notify`'s writeable branch re-arms reads explicitly, gated on
`_writeable` (true only when NOT backpressured, which also makes the combined
resubscribe touch only the read arm). On kqueue that re-arm is a harmless,
idempotent no-op.

The coupling: this re-arm is load-bearing ONLY while epoll and Windows deliver
read and write on one shared one-shot registration. If either backend is changed
to give each direction its own independent one-shot (as kqueue already does), the
explicit read re-arm in `_event_notify` becomes an inert no-op there and its
rationale changes; if a backend drops one-shot for edge/level-triggered
persistent, the whole re-arm dance changes. A change to the one-shot model in
`epoll.c` or `sock_notify.c` must be checked against `_event_notify`'s writeable
branch in `tcp_connection.pony`.

Run: the TCP tests in `packages/net/_test.pony`, and the tcp-swarm stress test
(`test/rt-stress/tcp-swarm/`) on Windows -- the swarm is what surfaced the hang,
and a moderate write-heavy workload (many queued writes per connection) reproduces
it probabilistically when the re-arm is missing.
