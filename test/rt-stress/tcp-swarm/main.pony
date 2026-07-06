"""
Swarm TCP stress engine.

A closed, count-driven TCP workload for stressing the runtime's net stack. Every
behaviour is a CLI flag set by the orchestrator; the engine draws nothing and sets
no runtime defaults. A fixed number of client connections is churned through a
listener at a bounded concurrency; each client sends a stamped payload, the server
echoes it, and the client verifies the echo byte-for-byte before closing.

The swarm dimensions are each tied to a distinct code path in
`packages/net/tcp_connection.pony` (see test/rt-stress/tcp-swarm/README.md):

* `--payload-size` / `--messages` -- how much each connection sends, and in how
  many messages.
* `--write-shape` (`write` | `writev`) -- single vs vectored writes.
* `--writev-chunks` (N, writev only) -- how many buffers a `writev` splits its
  payload into. Above `@pony_os_writev_max()` -- IOV_MAX (1024 on Linux/macOS) on
  POSIX, 1 on Windows -- the send takes TCPConnection's multi-batch path, sending
  one
  `writev_max`-sized batch per pass and re-entering the send loop, which is the
  only thing that makes the mid-write yield below actually fire on POSIX.
* `--expect` (0 = off, N = frame size) -- fixed-size framed reads vs whole-buffer.
* `--close` (`graceful` | `hard`) -- a graceful `dispose()` (FIN, drains the send
  buffer) vs a muted `dispose()`, which takes TCPConnection's `hard_close` path
  (immediate teardown, no lingering drain). The client closes only after its whole
  echo is back, so the hard path drops no data here -- it exercises the distinct
  teardown/unsubscribe code, not write loss.
* `--read-buffer-size` / `--yield-after-reading` / `--yield-after-writing` -- the
  TCPConnection read-buffer size and the byte counts at which it yields back to the
  scheduler mid-read/mid-write. A small `--yield-after-reading` yields on any
  payload big enough to fill the read buffer more than once. `--yield-after-writing`
  is different: the send loop only re-checks it when a write spans more than one
  `writev_max` batch, so on POSIX it needs a `--writev-chunks` above IOV_MAX to
  bite at all (on Windows, where writev_max is 1, any multi-chunk writev triggers
  it).

Oracles:

* Echo integrity -- each connection sends a unique, non-repeating byte stream
  (byte at stream position p is a splitmix64 hash of (connection-id, p)), and
  verifies every echoed byte against it. Because the stream is unique per
  connection and never repeats, this catches not only a corrupted byte or a short
  read but a byte delivered out of order, duplicated, or from another connection.
  Every connection must verify: the client closes only after it has read its whole
  echo back, so even a hard close tears down an already-drained connection. Fewer
  verified than spawned is a failure (see `_report`).
* Conservation -- every spawned connection reaches a terminal state
  (closed or connect-failed); the engine reports the tally.
* Crash / assert -- debug build, asserts on.

On success (every connection verified) the engine prints its RESULT line and PASS,
then returns, letting the program reach natural quiescence (clean runtime shutdown
is itself exercised). Anything short of full verification -- a connect failure, a
short echo, or a byte mismatch -- prints FAIL and forces a non-zero exit.
"""
use "collections"
use "net"
use "time"

use @printf[I32](fmt: Pointer[U8] tag, ...)
use @fprintf[I32](stream: Pointer[U8] tag, fmt: Pointer[U8] tag, ...)
use @fflush[I32](stream: Pointer[U8] tag)
use @pony_os_stdout[Pointer[U8]]()
use @pony_os_stderr[Pointer[U8]]()
use @exit[None](status: I32)

primitive _Flags
  """
  Parse `--key value` pairs (and bare `--key` as "true") from the program args.
  The runtime strips its own `--pony*` flags before main, so only workload flags
  arrive here; unknown keys are ignored by _Config.
  """
  fun apply(args: Array[String] box): Map[String, String] val =>
    let out = recover Map[String, String] end
    var i: USize = 1
    while i < args.size() do
      try
        let arg = args(i)?
        if arg.at("--") then
          let key = arg.substring(2)
          if ((i + 1) < args.size()) and (not args(i + 1)?.at("--")) then
            out(consume key) = args(i + 1)?
            i = i + 2
          else
            out(consume key) = "true"
            i = i + 1
          end
        else
          i = i + 1
        end
      else
        i = i + 1
      end
    end
    consume out

class val _Config
  let host: String
  let port: String
  let connections: USize
  let concurrency: USize
  let payload_size: USize
  let messages: USize
  let close_hard: Bool
  let use_writev: Bool
  let writev_chunks: USize
  let expect_frame: USize
  let read_buffer_size: USize
  let yield_after_reading: USize
  let yield_after_writing: USize

  new val create(m: Map[String, String] val) =>
    // "localhost", not "127.0.0.1": the literal v4 address makes macOS wall the
    // client at ~16k ephemeral ports mid-run (connections then fail, which this
    // test treats as a failure); "localhost" sidesteps it. This mirrors the
    // retired tcp-open-close test, which hit the same wall. See also the macOS
    // networking step in the stress workflow (macOS-configure-networking.bash).
    host = _str(m, "host", "localhost")
    port = _str(m, "port", "0")
    connections = _usize(m, "connections", 1000)
    // Floor concurrency at 1: 0 would spawn nothing yet never finish (a silent
    // hang the watchdog would catch as a false timeout).
    concurrency = _usize(m, "concurrency", 64).max(1)
    payload_size = _usize(m, "payload-size", 64)
    messages = _usize(m, "messages", 1)
    close_hard = _str(m, "close", "graceful") == "hard"
    use_writev = _str(m, "write-shape", "write") == "writev"
    // How many buffers a single `writev` splits its payload into. Above
    // `@pony_os_writev_max()` -- IOV_MAX (1024 on Linux/macOS) on POSIX, 1 on
    // Windows -- it drives TCPConnection's multi-batch send and the mid-write
    // yield, so on POSIX it bites only when payload_size >= writev_chunks > 1024.
    // Default 4; writev only. Coupling:
    // .known-couplings/tcp-swarm-writev-chunks-iov-max.md.
    writev_chunks = _usize(m, "writev-chunks", 4).max(1)
    read_buffer_size = _usize(m, "read-buffer-size", 16384)
    // Clamp expect to the read buffer: TCPConnection.expect() errors when the frame
    // exceeds it, so clamping here keeps the call from erroring (the call sites then
    // assert that with _Unreachable). The orchestrator already draws
    // expect <= read-buffer; this clamp only makes a directly-run engine safe from
    // that one error. NOTE: it does NOT save a directly-run engine from a
    // non-dividing --expect: for framed reads to terminate, payload_size * messages
    // must be a whole number of frames, or the trailing partial frame is never
    // delivered and the client hangs. The orchestrator guarantees divisibility by
    // drawing only power-of-two sizes; a hand-run engine must arrange it itself.
    expect_frame = _usize(m, "expect", 0).min(read_buffer_size)
    yield_after_reading = _usize(m, "yield-after-reading", 16384)
    yield_after_writing = _usize(m, "yield-after-writing", 16384)

  fun tag _str(m: Map[String, String] val, key: String, default: String)
    : String
  =>
    try m(key)? else default end

  fun tag _usize(m: Map[String, String] val, key: String, default: USize)
    : USize
  =>
    try m(key)?.usize()? else default end

primitive _Keystream
  """
  The echo oracle checks a unique, non-repeating byte stream per connection: the
  byte at stream position `p` is the low 8 bits of a splitmix64 hash of
  (seed, p). `seed` identifies the connection. Because the stream is unique per
  connection and does not repeat, a byte that arrives out of order, duplicated, or
  from another connection fails the check -- not only a corrupted or truncated one.
  It is generated per position (no template to bulk-copy), which is the price of a
  stream that is meant to be unique everywhere; the per-run byte volume is bounded
  by the orchestrator so the build is not the bottleneck.
  """
  fun byte(seed: U64, p: USize): U8 =>
    var z: U64 = seed + (p.u64() * 0x9E3779B97F4A7C15)
    z = (z xor (z >> 30)) * 0xBF58476D1CE4E5B9
    z = (z xor (z >> 27)) * 0x94D049BB133111EB
    (z xor (z >> 31)).u8()

  fun make(seed: U64, start: USize, len: USize): Array[U8] iso^ =>
    recover
      let a = Array[U8](len)
      var i: USize = 0
      while i < len do
        a.push(byte(seed, start + i))
        i = i + 1
      end
      a
    end

  fun make_chunks(seed: U64, start: USize, total: USize, nchunks: USize)
    : Array[ByteSeq] val
  =>
    """
    The same stream as `make`, split across `nchunks` buffers for a vectored
    `writev` -- contiguous over `start .. start + total`, so the echo verifies
    identically regardless of write shape.
    """
    // COUPLING: this allocates `nchunks` buffer objects per call regardless of
    // payload size (the extras are zero-length when nchunks > total). The
    // orchestrator's memory budget models peak memory as
    // OBJ_BYTES * concurrency * messages * writev_chunks off exactly this count
    // (est_peak_bytes in orchestrate_tcp.py) -- change how many objects this creates
    // and the budget can let an OOM through. See
    // .known-couplings/tcp-swarm-memory-budget.md.
    recover val
      let out = Array[ByteSeq]
      let n = if nchunks == 0 then 1 else nchunks end
      var done: USize = 0
      var c: USize = 0
      while c < n do
        let this_len = if c == (n - 1) then total - done else total / n end
        out.push(recover val
          let b = Array[U8](this_len)
          var i: USize = 0
          while i < this_len do
            b.push(byte(seed, start + done + i))
            i = i + 1
          end
          b
        end)
        done = done + this_len
        c = c + 1
      end
      out
    end

actor Spawner
  """
  Drives the run. Once the listener is up it keeps `concurrency` connections in
  flight at a time, refilling as each finishes, until `connections` have been
  spawned. It tallies every connection's terminal state (verified, mismatched,
  short, connect-failed), emits a heartbeat carrying the completed count on a fixed
  wall-clock timer (the orchestrator's watchdog reads that count and fails the run
  only if it stops advancing), and prints the pass/fail report when the last
  connection is accounted for.
  """
  let _config: _Config
  let _connect_auth: TCPConnectAuth
  var _port: String = ""
  var _listener: (TCPListener | None) = None
  var _started: Bool = false
  var _spawned: USize = 0
  var _inflight: USize = 0
  var _completed: USize = 0
  var _failed: USize = 0
  var _verified: USize = 0
  var _mismatched: USize = 0
  var _finished: Bool = false
  // Started in listener_ready, disposed at finish. See heartbeat_tick.
  let _timers: Timers = Timers

  new create(config: _Config, connect_auth: TCPConnectAuth) =>
    _config = config
    _connect_auth = connect_auth

  be listener_ready(listener: TCPListener, port: String) =>
    _listener = listener
    _port = port
    if not _started then
      _started = true
      // Heartbeat on a wall-clock timer, not per-completion: the orchestrator's
      // watchdog decides "hang" from whether `done` advances between heartbeats,
      // so liveness must be signalled on a fixed cadence a slow run can always
      // meet, independent of how fast connections complete. COUPLING: the interval
      // must stay well under the orchestrator's --no-progress-seconds window --
      // see .known-couplings/tcp-swarm-heartbeat-watchdog.md.
      let interval: U64 = 5_000_000_000  // 5s
      _timers(Timer(_HeartbeatTimer(this), interval, interval))
      _refill()
    end

  be connection_done(verified: Bool, mismatch: Bool) =>
    _inflight = _inflight - 1
    _completed = _completed + 1
    if verified then _verified = _verified + 1 end
    if mismatch then _mismatched = _mismatched + 1 end
    _refill()

  be connection_failed() =>
    _inflight = _inflight - 1
    _failed = _failed + 1
    _refill()

  be heartbeat_tick() =>
    // Fired by _HeartbeatTimer on a fixed wall-clock cadence. Prints the current
    // completed count so the orchestrator can see progress advancing; a run that
    // has stopped completing connections stops advancing `done` (the line keeps
    // coming), which is how the watchdog tells a slow run from a hang.
    if not _finished then _emit_heartbeat() end

  fun _emit_heartbeat() =>
    let done = _completed + _failed
    // Flushed: a block-buffered line would not reach the watchdog until the buffer
    // fills, which would defeat the no-progress detection.
    @printf("HEARTBEAT done=%zu of %zu\n".cstring(), done, _config.connections)
    @fflush(@pony_os_stdout())

  fun ref _refill() =>
    while (_inflight < _config.concurrency) and (_spawned < _config.connections) do
      TCPConnection(_connect_auth,
        SwarmClient(this, _config, _spawned),
        _config.host,
        _port,
        "",
        _config.read_buffer_size,
        _config.yield_after_reading,
        _config.yield_after_writing)
      _spawned = _spawned + 1
      _inflight = _inflight + 1
    end
    _try_finish()

  fun ref _try_finish() =>
    if (not _finished)
      and (_spawned >= _config.connections)
      and (_inflight == 0)
    then
      _finished = true
      // A final heartbeat with the true completed count before the timer stops: the
      // last wave completes after the previous tick, so this records the full total
      // and resets the watchdog's clock at finish (the shutdown that follows gets a
      // fresh no-progress window).
      _emit_heartbeat()
      // Stop the heartbeat timer so the runtime can reach quiescence: a live
      // repeating timer is a noisy ASIO event that would keep the program from
      // exiting after the last connection is done.
      _timers.dispose()
      _report()
      match _listener
      | let l: TCPListener => l.dispose()
      end
      _listener = None
    end

  fun _report() =>
    // %zu (size_t) is the portable format for USize -- %lu is 32-bit on Windows.
    @printf(("RESULT connections=%zu spawned=%zu completed=%zu failed=%zu "
      + "verified=%zu mismatched=%zu\n").cstring(),
      _config.connections, _spawned, _completed, _failed, _verified,
      _mismatched)
    // This is a stress test, not fault injection: every connection must connect,
    // exchange, and verify its echo. Anything less is a failure -- a connect that
    // failed (a bug, or a mis-set-up harness exhausting ports), a short echo (a
    // connection that closed with fewer bytes than it sent), or a byte mismatch.
    // All of them leave verified < connections.
    if _verified == _config.connections then
      @printf("PASS\n".cstring())
    else
      let truncated = (_completed - _verified) - _mismatched
      @printf(("FAIL: %zu of %zu connections did not verify "
        + "(connect_failed=%zu truncated=%zu mismatched=%zu)\n").cstring(),
        _config.connections - _verified, _config.connections,
        _failed, truncated, _mismatched)
      @exit(1)
    end

class _HeartbeatTimer is TimerNotify
  """
  Fires the Spawner's wall-clock heartbeat. Repeats on a fixed interval until the
  Spawner disposes the timer when the run finishes.
  """
  let _spawner: Spawner

  new iso create(spawner: Spawner) =>
    _spawner = spawner

  fun ref apply(timer: Timer, count: U64): Bool =>
    _spawner.heartbeat_tick()
    true

class SwarmListener is TCPListenNotify
  """
  The listen side. Hands each accepted connection an `EchoServer`, and once the
  listener has a bound port tells the `Spawner` to start dialing.
  """
  let _spawner: Spawner
  let _config: _Config

  new iso create(spawner: Spawner, config: _Config) =>
    _spawner = spawner
    _config = config

  fun ref connected(listen: TCPListener ref): TCPConnectionNotify iso^ =>
    EchoServer(_config)

  fun ref listening(listen: TCPListener ref) =>
    let port = recover val listen.local_address().port().string() end
    _spawner.listener_ready(listen, port)

  fun ref not_listening(listen: TCPListener ref) =>
    @printf("FAIL: listener could not start\n".cstring())
    @exit(1)

class EchoServer is TCPConnectionNotify
  """
  The server half of a connection: echoes every byte it receives straight back,
  unmodified. The client's keystream oracle checks what comes back, so the
  server stays deliberately dumb -- any corruption it introduced would be
  indistinguishable from a real stack bug.
  """
  let _config: _Config

  new iso create(config: _Config) =>
    _config = config

  fun ref accepted(conn: TCPConnection ref) =>
    if _config.expect_frame > 0 then
      // Unreachable error: expect() errors only when the frame exceeds the
      // connection's read buffer, and _Config clamps expect_frame to it (both
      // endpoints are created with read_buffer_size).
      try conn.expect(_config.expect_frame)? else _Unreachable() end
    end

  fun ref received(conn: TCPConnection ref, data: Array[U8] iso, times: USize)
    : Bool
  =>
    conn.write(consume data)
    true

  fun ref connect_failed(conn: TCPConnection ref) =>
    None

class SwarmClient is TCPConnectionNotify
  """
  The client half of a connection. On connect it writes its whole keystream --
  `messages` payloads of `payload_size` bytes, via `write` or `writev` -- then
  verifies the echo byte for byte against the same keystream as it comes back.
  When it has read back everything it sent it closes (gracefully, or muted for a
  hard close) and reports to the `Spawner` whether the echo verified.
  """
  let _spawner: Spawner
  let _config: _Config
  let _seed: U64
  var _sent_total: USize = 0
  var _recv_count: USize = 0
  var _mismatch: Bool = false
  var _reported: Bool = false

  new iso create(spawner: Spawner, config: _Config, id: USize) =>
    _spawner = spawner
    _config = config
    // The connection id keys its own byte stream; distinct ids give distinct,
    // unrelated streams (splitmix diffuses adjacent seeds), so a byte from
    // another connection fails this connection's check.
    _seed = id.u64()

  fun ref connected(conn: TCPConnection ref) =>
    if _config.expect_frame > 0 then
      // Unreachable error: expect() errors only when the frame exceeds the
      // connection's read buffer, and _Config clamps expect_frame to it (both
      // endpoints are created with read_buffer_size).
      try conn.expect(_config.expect_frame)? else _Unreachable() end
    end

    var m: USize = 0
    while m < _config.messages do
      if _config.use_writev then
        // Split across `writev_chunks` buffers to drive the vectored-write path.
        // Several non-empty iovecs need payload_size >= writev_chunks; a chunk
        // count above the payload collapses to a single iovec (writev skips the
        // empty buffers). A chunk count above IOV_MAX drives the multi-batch send.
        conn.writev(_Keystream.make_chunks(
          _seed, _sent_total, _config.payload_size, _config.writev_chunks))
      else
        conn.write(_Keystream.make(_seed, _sent_total, _config.payload_size))
      end
      _sent_total = _sent_total + _config.payload_size
      m = m + 1
    end

    if _sent_total == 0 then
      _close(conn)
    end

  fun ref received(conn: TCPConnection ref, data: Array[U8] iso, times: USize)
    : Bool
  =>
    let n = data.size()
    try
      var i: USize = 0
      while i < n do
        if data(i)? != _Keystream.byte(_seed, _recv_count + i) then
          _mismatch = true
        end
        i = i + 1
      end
    else
      // Unreachable: i < n == data.size() bounds every access.
      _Unreachable()
    end
    _recv_count = _recv_count + n

    if _recv_count >= _sent_total then
      _close(conn)
      false
    else
      true
    end

  fun ref connect_failed(conn: TCPConnection ref) =>
    if not _reported then
      _reported = true
      _spawner.connection_failed()
    end

  fun ref closed(conn: TCPConnection ref) =>
    if not _reported then
      _reported = true
      let verified = (not _mismatch) and (_recv_count >= _sent_total)
      _spawner.connection_done(verified, _mismatch)
    end

  fun ref _close(conn: TCPConnection ref) =>
    if _config.close_hard then
      conn.mute()
    end
    conn.dispose()

actor Main
  """
  Parses the flags into a `_Config`, stands up the echo listener, and starts the
  run. All the work happens in the `Spawner` and the per-connection notifiers.
  """
  new create(env: Env) =>
    let config = _Config(_Flags(env.args))
    let spawner = Spawner(config, TCPConnectAuth(env.root))
    TCPListener(TCPListenAuth(env.root),
      SwarmListener(spawner, config),
      config.host,
      config.port,
      0,
      config.read_buffer_size,
      config.yield_after_reading,
      config.yield_after_writing)

primitive _Unreachable
  """
  For a branch the compiler forces us to write but that we know is dead: if it is
  ever reached, crash with the source location rather than silently continuing on
  corrupt state.
  """
  fun apply(loc: SourceLoc = __loc) =>
    @fprintf(@pony_os_stderr(),
      "Reached unreachable code at %s:%s\n".cstring(),
      loc.file().cstring(), loc.line().string().cstring())
    @exit(1)
