# UDP empty-datagram delivery depends on the read loop charging 1 byte per read

`pony_os_recvfrom` in `src/libponyrt/lang/socket.c` returns `PONY_SOCKET_OK`
with `*count_out == 0` for a zero-length UDP datagram (valid per RFC 768). The
stdlib read loop `UDPSocket._pending_reads` in `packages/net/udp_socket.pony`
relies on that: it delivers the empty datagram, then charges its read budget
`count.max(1)` rather than `count`.

The two are coupled. The runtime side makes a zero-count `OK` a normal,
repeatable result. The stdlib side must charge at least 1 byte per read, or the
`sum > (1 << 12)` yield threshold is never reached under a sustained flood of
empty datagrams: `sum` stays 0, the `while _readable` loop never calls
`_read_again`, and the actor spins inside a single behavior run, starving the
other actors on its scheduler thread. Simplifying `count.max(1)` back to
`count`, or changing the runtime so recvfrom no longer returns a zero-count
`OK`, reintroduces the starvation.

Only the runtime end is guarded by a test.
`SocketRecvTest.RecvFromOnEmptyDatagramReturnsOk` in
`test/libponyrt/lang/socket.cc` fails if `pony_os_recvfrom` stops returning a
zero-count `OK` for an empty datagram.

Nothing guards the stdlib end. That failure is a scheduler-fairness property,
not an output: reproducing it needs thousands of empty datagrams resident in
the socket buffer and arriving faster than they drain, and a "blast N datagrams
and assert the test finishes" check passes with or without the guard. The
`count.max(1)` charge is verified by reasoning, so this file is the record. If a
test seam is ever added over the read accumulator, cover it there.

Run: `ctest --preset debug -R libponyrt.tests` (catches a regression on the
runtime end only)
