"""
UDP flood stress engine.

A count-driven, one-way UDP workload for stressing the UDP stack. A fixed
number of clients send stamped datagrams to a server; the server verifies the
payload of each received datagram against a per-client keystream and reports
results via actor messaging. There is no UDP echo: the only UDP traffic is
client-to-server.

The point is to exercise the readiness event delivery path under sustained
datagram volume, verifying that the ASIO backend (ProcessSocketNotifications
on Windows, epoll on Linux, kqueue on macOS) correctly delivers persistent
edge-triggered notifications for UDP sockets across many read-loop re-entries.

Each swarm dimension exercises a distinct code path in `udp_socket.pony`:

* `--datagrams` / `--payload-size` -- volume and per-datagram size.
* `--batch-size` -- how many datagrams a client sends per scheduling turn
  before yielding. The client sends continuously until all datagrams are sent.
* `--clients` -- concurrent client sockets sending to the same server.
* `--read-buffer-size` -- the per-socket read buffer, which sets the byte
  budget in `_pending_reads`.
* `--max-datagrams-per-turn` -- the per-turn datagram ceiling in
  `_pending_reads`. Small values exercise the `_read_again` yield path.

Payload format:

Each datagram carries a 4-byte header followed by keystream data:
  byte 0:   client id (U8)
  bytes 1-3: sequence number (big-endian, 3 bytes)
  bytes 4+:  `_Keystream.make(seed, start, payload_size - _HeaderSize())`

where `seed` is the client id and `start` is `seq * (payload_size -
_HeaderSize())`. For payloads smaller than `_HeaderSize() + 1` the entire
datagram is keystream with no header, and the server counts it without
verifying integrity.

Oracles:

* Payload integrity -- the server reads the header, regenerates the expected
  keystream, and compares. A mismatch is corruption.
* Crash / assert -- debug build, asserts on.

On success (every invariant holds) the engine prints its RESULT line and PASS,
then returns. Anything short of that prints FAIL with the specific violation
and exits non-zero.
"""
