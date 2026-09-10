use "net"
use "cli"
use "constrained_types"
use "time"

use @printf[I32](fmt: Pointer[U8] tag, ...)
use @fprintf[I32](stream: Pointer[U8] tag, fmt: Pointer[U8] tag, ...)
use @fflush[I32](stream: Pointer[U8] tag)
use @pony_os_stdout[Pointer[U8]]()
use @pony_os_stderr[Pointer[U8]]()
use @exit[None](status: I32)

primitive _HeaderSize
  fun apply(): USize => 4

primitive _Flag
  """
  The command-line flag names, in one place. `_FloodSpec` (which declares them)
  and `_MakeConfig` (which reads them) both name flags through here, so a
  mistyped name is a compile error rather than a silently ignored option or a
  value that reads back as zero.
  """
  fun host(): String => "host"
  fun port(): String => "port"
  fun datagrams(): String => "datagrams"
  fun payload_size(): String => "payload-size"
  fun batch_size(): String => "batch-size"
  fun clients(): String => "clients"
  fun read_buffer_size(): String => "read-buffer-size"
  fun max_datagrams_per_turn(): String => "max-datagrams-per-turn"

primitive _FloodSpec
  """
  The command-line schema: every workload flag, its type, and its default.
  `Main` parses `env.args` against this, so a malformed or misspelled flag is
  a reported error rather than a silent default. The `cli` parser checks types
  and flag names; `_MakeConfig` checks value domains.
  """
  fun apply(): CommandSpec ? =>
    CommandSpec.leaf(
      "udp-flood",
      "UDP flood stress engine.",
      [ OptionSpec.string(
          _Flag.host(),
          "server bind host"
          where default' = "localhost")
        OptionSpec.string(
          _Flag.port(),
          "server bind port (0 = ephemeral)"
          where default' = "0")
        OptionSpec.u64(
          _Flag.datagrams(),
          "datagrams each client sends"
          where default' = 1000)
        OptionSpec.u64(
          _Flag.payload_size(),
          "bytes per datagram"
          where default' = 64)
        OptionSpec.u64(
          _Flag.batch_size(),
          "datagrams per send burst"
          where default' = 10)
        OptionSpec.u64(
          _Flag.clients(),
          "concurrent client sockets"
          where default' = 4)
        OptionSpec.u64(
          _Flag.read_buffer_size(),
          "per-socket read buffer size"
          where default' = 16384)
        OptionSpec.u64(
          _Flag.max_datagrams_per_turn(),
          "datagram ceiling per read turn"
          where default' = 256)
      ])? .> add_help(where descr' = "print usage and exit")?

class val _Config
  """
  The validated workload configuration. Built only by `_MakeConfig`, from a
  parsed command line whose every value it has already checked -- so the rest
  of the engine can trust these fields without re-checking.
  """
  let host: String
  let port: String
  let datagrams: USize
  let payload_size: USize
  let batch_size: USize
  let clients: USize
  let read_buffer_size: USize
  let max_datagrams_per_turn: USize

  new val _create(
    host': String,
    port': String,
    datagrams': USize,
    payload_size': USize,
    batch_size': USize,
    clients': USize,
    read_buffer_size': USize,
    max_datagrams_per_turn': USize)
  =>
    host = host'
    port = port'
    datagrams = datagrams'
    payload_size = payload_size'
    batch_size = batch_size'
    clients = clients'
    read_buffer_size = read_buffer_size'
    max_datagrams_per_turn = max_datagrams_per_turn'

  fun read_buffer(): ReadBufferSize =>
    match MakeReadBufferSize(read_buffer_size)
    | let r: ReadBufferSize => r
    else
      _Unreachable()
      DefaultReadBufferSize()
    end

primitive _MakeConfig
  """
  Reads and validates the workload flags off a parsed `Command`, returning a
  `_Config` when every value is in range, or a `ValidationFailure` naming the
  first bad one. Validation lives here, at the construction boundary, because
  `Main` still holds the `Env` to report the failure -- a `_Config` cannot.
  """
  fun apply(cmd: Command box): (_Config | ValidationFailure) =>
    let datagrams = cmd.option(_Flag.datagrams()).u64().usize()
    if datagrams < 1 then
      return recover val
        ValidationFailure("--datagrams must be at least 1")
      end
    end
    if datagrams > 16777215 then
      return recover val
        ValidationFailure(
          "--datagrams must not exceed 16777215 (sequence is three bytes)")
      end
    end
    let payload_size = cmd.option(_Flag.payload_size()).u64().usize()
    if payload_size < 1 then
      return recover val
        ValidationFailure("--payload-size must be at least 1")
      end
    end
    let batch_size = cmd.option(_Flag.batch_size()).u64().usize()
    if batch_size < 1 then
      return recover val
        ValidationFailure("--batch-size must be at least 1")
      end
    end
    let clients = cmd.option(_Flag.clients()).u64().usize()
    if clients < 1 then
      return recover val
        ValidationFailure("--clients must be at least 1")
      end
    end
    if clients > 255 then
      return recover val
        ValidationFailure(
          "--clients must not exceed 255 (client id is one byte)")
      end
    end
    let read_buffer_size = cmd.option(_Flag.read_buffer_size()).u64().usize()
    if read_buffer_size < 1 then
      return recover val
        ValidationFailure("--read-buffer-size must be at least 1")
      end
    end
    try
      payload_size.mul_partial(datagrams)?
    else
      return recover val
        ValidationFailure("--payload-size * --datagrams overflows USize")
      end
    end
    if payload_size > read_buffer_size then
      return recover val
        ValidationFailure(
          "--payload-size (" + payload_size.string() +
            ") must not exceed --read-buffer-size (" +
            read_buffer_size.string() + ")")
      end
    end
    let max_datagrams_per_turn =
      cmd.option(_Flag.max_datagrams_per_turn()).u64().usize()
    if max_datagrams_per_turn < 1 then
      return recover val
        ValidationFailure("--max-datagrams-per-turn must be at least 1")
      end
    end

    _Config._create(
      cmd.option(_Flag.host()).string(),
      cmd.option(_Flag.port()).string(),
      datagrams,
      payload_size,
      batch_size,
      clients,
      read_buffer_size,
      max_datagrams_per_turn)

primitive _Keystream
  """
  Pseudo-random byte stream keyed by (seed, position). The byte at stream
  position `p` is the low 8 bits of a splitmix64 hash of (seed, p). Used to
  generate datagram payloads and to verify them on the server side.
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

primitive _Checksum
  """
  FNV-1a 64-bit hash. Used by the keystream self-check.
  """
  fun from_keystream(seed: U64, start: USize, len: USize): U64 =>
    var h: U64 = 14695981039346656037
    var i: USize = 0
    while i < len do
      h = (h xor _Keystream.byte(seed, start + i).u64()) * 1099511628211
      i = i + 1
    end
    h

primitive _KeystreamSelfCheck
  """
  Guards the oracle's core before the run. The server verifies payloads using
  `_Keystream`, so a degenerate keystream (constant output, or one that ignores
  the seed) would make every payload verify against matching-but-wrong data.
  """
  fun apply() =>
    var seeds_differ = false
    var p: USize = 0
    while p < 256 do
      if _Keystream.byte(0, p) != _Keystream.byte(1, p) then
        seeds_differ = true
        break
      end
      p = p + 1
    end
    var seed_varies = false
    let first = _Keystream.byte(0, 0)
    var q: USize = 1
    while q < 256 do
      if _Keystream.byte(0, q) != first then
        seed_varies = true
        break
      end
      q = q + 1
    end
    if not (seeds_differ and seed_varies) then
      @printf("FAIL: keystream self-check\n".cstring())
      @fprintf(
        @pony_os_stderr(),
        ("FATAL: _Keystream self-check failed" +
          " -- the integrity oracle is degenerate\n").cstring())
      @exit(1)
    end
    let h0 = _Checksum.from_keystream(0, 0, 64)
    let h1 = _Checksum.from_keystream(1, 0, 64)
    let h2 = _Checksum.from_keystream(0, 64, 64)
    if (h0 == h1) or (h0 == h2) then
      @printf("FAIL: checksum self-check\n".cstring())
      @fprintf(
        @pony_os_stderr(),
        "FATAL: _Checksum self-check failed -- the hash is degenerate\n"
          .cstring())
      @exit(1)
    end

primitive _Header
  """
  Encode and decode the 4-byte datagram header: 1 byte client id, 3 bytes
  big-endian sequence number.
  """
  fun encode(
    client_id: U8,
    seq: USize,
    payload_data: Array[U8] val)
    : Array[U8] val
  =>
    recover val
      let a = Array[U8](4 + payload_data.size())
      a.push(client_id)
      a.push((seq >> 16).u8())
      a.push((seq >> 8).u8())
      a.push(seq.u8())
      for b in payload_data.values() do
        a.push(b)
      end
      a
    end

  fun decode(data: Array[U8] box): (U8, USize) ? =>
    let client_id = data(0)?
    let seq =
      ((data(1)?.usize() << 16) or
        (data(2)?.usize() << 8) or
        data(3)?.usize())
    (client_id, seq)

actor Spawner
  """
  Coordinates server and client lifecycle. Prints the RESULT/PASS/FAIL line
  when all actors have reported.
  """
  let _config: _Config
  let _udp_auth: UDPAuth
  var _server: (FloodServer | None) = None
  let _clients: Array[FloodClient] = Array[FloodClient]
  var _started: Bool = false
  var _clients_done_sending: USize = 0
  var _total_client_sent: USize = 0
  var _server_received: USize = 0
  var _server_corrupted: USize = 0
  var _server_reported: Bool = false
  var _clients_reported: USize = 0
  var _total_client_send_errors: USize = 0
  var _total_client_bind_failed: USize = 0
  var _finished: Bool = false
  let _timers: Timers = Timers

  new create(config: _Config, udp_auth: UDPAuth) =>
    _config = config
    _udp_auth = udp_auth

  be server_ready(server: FloodServer, addr: NetAddress val) =>
    _server = server
    if not _started then
      _started = true
      let interval: U64 = 5_000_000_000
      _timers(Timer(_HeartbeatTimer(this), interval, interval))
      var i: USize = 0
      while i < _config.clients do
        let c = FloodClient(this, _config, i, _udp_auth, addr)
        _clients.push(c)
        i = i + 1
      end
    end

  be server_bind_failed() =>
    @printf("FAIL: server could not bind\n".cstring())
    @exit(1)

  be client_done_sending(sent: USize) =>
    """
    A client finished sending. Accumulates the sent count and, when all
    clients are done, tells the server to begin its idle-timer shutdown.
    """
    _total_client_sent = _total_client_sent + sent
    _clients_done_sending = _clients_done_sending + 1
    if _clients_done_sending >= _config.clients then
      match _server
      | let s: FloodServer => s.all_clients_done()
      end
    end

  be client_reported(send_error: Bool) =>
    _clients_reported = _clients_reported + 1
    if send_error then
      _total_client_send_errors = _total_client_send_errors + 1
    end
    _try_finish()

  be client_bind_failed() =>
    """
    A client could not bind its socket. Counts as both done-sending and
    reported so the termination flow is not blocked.
    """
    _total_client_bind_failed = _total_client_bind_failed + 1
    _clients_done_sending = _clients_done_sending + 1
    _clients_reported = _clients_reported + 1
    if _clients_done_sending >= _config.clients then
      match _server
      | let s: FloodServer => s.all_clients_done()
      end
    end
    _try_finish()

  be server_done(received: USize, corrupted: USize) =>
    """
    Report server-side tallies and trigger client finish.
    """
    _server_received = received
    _server_corrupted = corrupted
    _server_reported = true
    for c in _clients.values() do
      c.finish()
    end
    _try_finish()

  be heartbeat_tick() =>
    if not _finished then _emit_heartbeat() end

  fun _emit_heartbeat() =>
    @printf(
      "HEARTBEAT reported=%zu of %zu\n".cstring(),
      _clients_reported,
      _config.clients)
    @fflush(@pony_os_stdout())

  fun ref _try_finish() =>
    if (not _finished) and
      (_clients_reported >= _config.clients) and _server_reported
    then
      _finished = true
      _emit_heartbeat()
      _timers.dispose()
      _report()
      match _server
      | let s: FloodServer => s.dispose()
      end
      _server = None
    end

  fun _report() =>
    @printf(
      ("RESULT clients=%zu client_sent=%zu " +
        "server_received=%zu server_corrupted=%zu " +
        "client_send_errors=%zu bind_failed=%zu\n").cstring(),
      _config.clients,
      _total_client_sent,
      _server_received,
      _server_corrupted,
      _total_client_send_errors,
      _total_client_bind_failed)

    var pass = true
    if _total_client_bind_failed > 0 then
      @printf(
        "FAIL: %zu client(s) could not bind\n".cstring(),
        _total_client_bind_failed)
      pass = false
    end
    if _total_client_send_errors > 0 then
      @printf(
        "FAIL: %zu client(s) hit SendToError\n".cstring(),
        _total_client_send_errors)
      pass = false
    end
    if _server_corrupted > 0 then
      @printf(
        "FAIL: server detected %zu corrupted datagram(s)\n".cstring(),
        _server_corrupted)
      pass = false
    end
    if pass then
      @printf("PASS\n".cstring())
    else
      @exit(1)
    end

class _HeartbeatTimer is TimerNotify
  """
  Fires the Spawner's wall-clock heartbeat. Repeats on a fixed interval until
  the Spawner disposes the timer when the run finishes.
  """
  let _spawner: Spawner

  new iso create(spawner: Spawner) =>
    _spawner = spawner

  fun ref apply(timer: Timer, count: U64): Bool =>
    _spawner.heartbeat_tick()
    true

class _ServerIdleNotify is TimerNotify
  let _server: FloodServer
  let _gen: USize

  new iso create(server: FloodServer, gen: USize) =>
    _server = server
    _gen = gen

  fun ref apply(timer: Timer, count: U64): Bool =>
    _server.idle_expired(_gen)
    false

class _ClientFinishNotify is TimerNotify
  let _client: FloodClient
  let _gen: USize

  new iso create(client: FloodClient, gen: USize) =>
    _client = client
    _gen = gen

  fun ref apply(timer: Timer, count: U64): Bool =>
    _client.finish_expired(_gen)
    false

actor FloodServer is (UDPSocketActor & UDPLifecycleEventReceiver)
  """
  Receive-only server. Verifies each datagram's payload against the expected
  keystream and tracks received and corrupted counts. Reports to the Spawner
  when quiescent.
  """
  let _spawner: Spawner
  let _config: _Config
  var _udp: UDPSocket = UDPSocket.none()
  var _server_received: USize = 0
  var _server_corrupted: USize = 0
  var _all_done: Bool = false
  var _idle_gen: USize = 0
  var _idle_fired: Bool = false
  var _reported: Bool = false
  let _timers: Timers = Timers

  new create(spawner: Spawner, config: _Config, udp_auth: UDPAuth) =>
    _spawner = spawner
    _config = config
    _udp =
      UDPSocket(
        udp_auth, config.host, config.port, this, this, config.read_buffer()
        where max_datagrams_per_turn = config.max_datagrams_per_turn)

  fun ref _socket(): UDPSocket => _udp

  fun ref _on_bound() =>
    _udp.set_so_rcvbuf(4194304)
    let addr = _udp.local_address()
    _spawner.server_ready(this, addr)

  fun ref _on_bind_failure() =>
    _spawner.server_bind_failed()

  fun ref _on_received(data: Array[U8] iso, from: NetAddress val)
    : ReadAction
  =>
    if _reported then return KeepReading end
    _server_received = _server_received + 1
    let d: Array[U8] val = consume data
    if d.size() > _HeaderSize() then
      try
        (let client_id, let seq) = _Header.decode(d)?
        let seed = client_id.u64()
        let data_len = d.size() - _HeaderSize()
        let start = seq * data_len
        var i: USize = 0
        while i < data_len do
          let expected = _Keystream.byte(seed, start + i)
          if d(_HeaderSize() + i)? != expected then
            _server_corrupted = _server_corrupted + 1
            break
          end
          i = i + 1
        end
      else
        _server_corrupted = _server_corrupted + 1
      end
    end
    if _all_done then _arm_idle() end
    KeepReading

  fun ref _arm_idle() =>
    _idle_fired = false
    _idle_gen = _idle_gen + 1
    _timers(Timer(_ServerIdleNotify(this, _idle_gen), 50_000_000))

  be idle_expired(gen: USize) =>
    if (not _reported) and (gen == _idle_gen) then
      _idle_fired = true
      _try_report()
    end

  fun ref _try_report() =>
    if _idle_fired and (not _reported) then
      _reported = true
      _timers.dispose()
      _spawner.server_done(_server_received, _server_corrupted)
    end

  be all_clients_done() =>
    _all_done = true
    _arm_idle()

  fun ref _on_closed() =>
    None

actor FloodClient is (UDPSocketActor & UDPLifecycleEventReceiver)
  """
  Send-only client. Sends stamped datagrams to the server and reports the sent
  count to the Spawner. Does not receive UDP traffic; results come back via
  actor messaging.
  """
  let _spawner: Spawner
  let _config: _Config
  var _udp: UDPSocket = UDPSocket.none()
  let _client_id: U8
  let _seed: U64
  let _server_addr: NetAddress val
  var _send_cursor: USize = 0
  var _send_error: Bool = false
  var _reported: Bool = false
  var _done_sending: Bool = false
  var _finish_gen: USize = 0
  let _timers: Timers = Timers

  new create(
    spawner: Spawner,
    config: _Config,
    id: USize,
    udp_auth: UDPAuth,
    server_addr: NetAddress val)
  =>
    _spawner = spawner
    _config = config
    _client_id = id.u8()
    _seed = id.u64()
    _server_addr = server_addr
    _udp =
      UDPSocket(
        udp_auth, config.host, "0", this, this, config.read_buffer()
        where max_datagrams_per_turn = config.max_datagrams_per_turn)

  fun ref _socket(): UDPSocket => _udp

  fun ref _on_bound() =>
    _udp.set_so_sndbuf(1048576)
    _pump()

  fun ref _on_bind_failure() =>
    if not _reported then
      _reported = true
      _spawner.client_bind_failed()
    end

  fun ref _pump() =>
    var sent_this_turn: USize = 0
    while (sent_this_turn < _config.batch_size) and
      (_send_cursor < _config.datagrams)
    do
      let payload: Array[U8] val =
        if _config.payload_size > _HeaderSize() then
          let data_len = _config.payload_size - _HeaderSize()
          let start = _send_cursor * data_len
          _Header.encode(
            _client_id,
            _send_cursor,
            _Keystream.make(_seed, start, data_len))
        else
          _Keystream.make(
            _seed,
            _send_cursor * _config.payload_size,
            _config.payload_size)
        end
      match \exhaustive\ _udp.send_to(payload, _server_addr)
      | SendToOk =>
        _send_cursor = _send_cursor + 1
        sent_this_turn = sent_this_turn + 1
      | SendToWouldBlock =>
        _retry_send()
        return
      | SendToError =>
        _send_error = true
        _close_and_report()
        return
      | SendToNotOpen =>
        _close_and_report()
        return
      end
    end

    if _send_cursor >= _config.datagrams then
      if not _done_sending then
        _done_sending = true
        _spawner.client_done_sending(_send_cursor)
      end
    else
      _continue_sending()
    end

  be _retry_send() =>
    if not _reported then
      _pump()
    end

  be _continue_sending() =>
    if not _reported then
      _pump()
    end

  fun ref _on_received(data: Array[U8] iso, from: NetAddress val)
    : ReadAction
  =>
    KeepReading

  be finish() =>
    """
    The server has reported. Give a short grace period, then report.
    """
    _finish_gen = _finish_gen + 1
    _timers(Timer(_ClientFinishNotify(this, _finish_gen), 50_000_000))

  be finish_expired(gen: USize) =>
    if (not _reported) and (gen == _finish_gen) then
      _report()
    end

  fun ref _report() =>
    if not _reported then
      _reported = true
      _timers.dispose()
      if not _done_sending then
        _done_sending = true
        _spawner.client_done_sending(_send_cursor)
      end
      _spawner.client_reported(_send_error)
      _udp.close()
    end

  fun ref _close_and_report() =>
    _report()

  fun ref _on_closed() =>
    None

actor Main
  """
  Parses and validates the flags into a `_Config`, stands up the server, and
  starts the run.
  """
  new create(env: Env) =>
    _KeystreamSelfCheck()

    let spec =
      try
        _FloodSpec()?
      else
        _Unreachable()
        return
      end

    let cmd =
      match \exhaustive\ CommandParser(spec).parse(env.args)
      | let c: Command => c
      | let ch: CommandHelp =>
        ch.print_help(env.out)
        return
      | let se: SyntaxError =>
        env.err.print(se.string())
        env.exitcode(1)
        return
      end

    let config =
      match \exhaustive\ _MakeConfig(cmd)
      | let c: _Config => c
      | let vf: ValidationFailure =>
        for e in vf.errors().values() do
          env.err.print(e)
        end
        env.exitcode(1)
        return
      end

    let udp_auth = UDPAuth(env.root)
    let spawner = Spawner(config, udp_auth)
    FloodServer(spawner, config, udp_auth)

primitive _Unreachable
  """
  For a branch the compiler forces us to write but that we know is dead: if it
  is ever reached, crash with the source location rather than silently
  continuing on corrupt state.
  """
  fun apply(loc: SourceLoc = __loc) =>
    @fprintf(
      @pony_os_stderr(),
      ("Reached unreachable code at %s:%s\n" +
        "Please open an issue at https://github.com/ponylang/ponyc/issues\n")
        .cstring(),
      loc.file().cstring(),
      loc.line().string().cstring())
    @exit(1)
