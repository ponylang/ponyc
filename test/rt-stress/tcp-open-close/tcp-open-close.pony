use "collections"
use "net"
use "runtime_info"
use "time"

use @exit[None](status: U8)
use @fprintf[I32](stream: Pointer[U8] tag, fmt: Pointer[U8] tag, ...)
use @pony_os_stderr[Pointer[U8]]()

actor Main
  new create(env: Env) =>
    // Setting this to 127.0.0.1 instead of localhost triggers weird MacOS
    // behavior that results in us not being able to open client connections
    // after a while (somewhere around 16k).
    // See https://web.archive.org/web/20180129235834/http://danielmendel.github.io/blog/2013/04/07/benchmarkers-beware-the-ephemeral-port-limit/
    // Even with this change (that will probably help on a local machine), you
    // should seriously consider making the changes that we do in CI to run
    // this on macOS. See macOS-configure-networking.bash.
    let host = "localhost"
    let port = "7669"
    let max_concurrent_connections =
      Scheduler.schedulers(SchedulerInfoAuth(env.root))
    let total_clients_to_spawn = try
      env.args(1)?.usize()?
    else
      250_000
    end

    let logger = Logger(env.out)

    logger.log("Will spawn a total of "
      + total_clients_to_spawn.string()
      + " clients.")
    logger.log("Max concurrent connections is "
      + max_concurrent_connections.string())

    let verbose = ifdef windows then
      true
    else
      false
    end

    let spawner = ClientSpawner(max_concurrent_connections,
      total_clients_to_spawn,
      TCPConnectAuth(env.root),
      host,
      port,
      logger,
      verbose)

    TCPListener(TCPListenAuth(env.root),
      Listener(max_concurrent_connections, logger, verbose, spawner),
      host,
      port)

  fun @runtime_override_defaults(rto: RuntimeOptions) =>
     rto.ponynoscale = true

class Listener is TCPListenNotify
  let _max_concurrent_connections: U32
  let _logger: Logger
  let _verbose: Bool
  let _spawner: ClientSpawner

  new iso create(max_concurrent_connections: U32,
    logger: Logger,
    verbose: Bool,
    spawner: ClientSpawner)
  =>
    _max_concurrent_connections = max_concurrent_connections
    _logger = logger
    _verbose = verbose
    _spawner = spawner

  fun ref connected(listen: TCPListener ref): TCPConnectionNotify iso^ =>
    Server(_logger, _verbose)

  fun ref closed(listen: TCPListener ref) =>
    _logger.log("Listener shut down.")

  fun ref not_listening(listen: TCPListener ref) =>
    _logger.log("Couldn't start listener.")

  fun ref listening(listen: TCPListener ref) =>
    _logger.log("Listener started.")
    _spawner.start(listen)

class Server is TCPConnectionNotify
  let _logger: Logger
  let _verbose: Bool

  new iso create(logger: Logger, verbose: Bool) =>
    _logger = logger
    _verbose = verbose

  fun ref received(
    conn: TCPConnection ref,
    data: Array[U8] iso,
    times: USize)
    : Bool
  =>
    if _verbose then
      _logger.log("Server received data.")
    end
    conn.write(consume data)
    false

  fun ref connect_failed(conn: TCPConnection ref) =>
    _Unreachable()

class Client is TCPConnectionNotify
  let _spawner: ClientSpawner
  let _logger: Logger
  let _verbose: Bool

  new iso create(spawner: ClientSpawner, logger: Logger, verbose: Bool) =>
    _spawner = spawner
    _logger = logger
    _verbose = verbose

  fun ref connected(conn: TCPConnection ref) =>
    if _verbose then
      _logger.log("Client Connected.")
    end

    conn.write("Hi there!")

  fun ref connect_failed(conn: TCPConnection ref) =>
    if _verbose then
      _logger.log("Client Connection Failure.")
    end

    _spawner.failed(conn)

  fun ref received(
    conn: TCPConnection ref,
    data: Array[U8] iso,
    times: USize)
    : Bool
  =>
    if _verbose then
      _logger.log("Client Received Data.")
    end

    conn.close()
    false

  fun ref closed(conn: TCPConnection ref) =>
    _spawner.closed(conn)

actor ClientSpawner
  let _max_concurrent_clients: U32
  let _total_clients_to_spawn: USize
  let _auth: TCPConnectAuth
  let _host: String
  let _port: String
  let _logger: Logger
  let _verbose: Bool
  var _clients_spawned: SetIs[TCPConnection] = SetIs[TCPConnection].create()
  var _total_clients_spawned: USize = 0
  var _listener: (TCPListener | None) = None
  var _started: Bool = false
  var _failed: USize = 0
  var _finished: Bool = false
  var _last_size: USize = 0
  let _report_every: USize

  new create(max_concurrent_clients: U32,
    total_clients_to_spawn: USize,
    auth: TCPConnectAuth,
    host: String,
    port: String,
    logger: Logger,
    verbose: Bool)
  =>
    _max_concurrent_clients = max_concurrent_clients
    _total_clients_to_spawn = total_clients_to_spawn
    _auth = auth
    _host = host
    _port = port
    _logger = logger
    _verbose = verbose
    _report_every = _total_clients_to_spawn / 20

  be start(listener: TCPListener) =>
    if not _started then
      _logger.log("Starting client spawner.")
      _started = true
      _listener = listener
      _spawn()
    else
      _Unreachable()
    end

  be closed(client: TCPConnection) =>
    if _verbose then
      _logger.log("Client closed.")
    end
    _clients_spawned.unset(client)
    if _should_continue() then
      _spawn()
    end

  be failed(client: TCPConnection) =>
    if _verbose then
      _logger.log("Client failed.")
    end
    _failed = _failed + 1
    _clients_spawned.unset(client)
    if _should_continue() then
      _spawn()
    end

  fun ref _spawn() =>
    while _should_spawn() do
      let c = TCPConnection(_auth,
        Client(this, _logger, _verbose),
        _host,
        _port)
      _clients_spawned.set(c)
      _total_clients_spawned = _total_clients_spawned + 1
    end
    _print_progress()
    _try_shutdown()

  fun _should_spawn(): Bool =>
    (_clients_spawned.size() < _max_concurrent_clients.usize()) and
    _should_continue()

  fun _should_continue(): Bool =>
    _total_clients_spawned < _total_clients_to_spawn

  fun _print_progress() =>
    if (_total_clients_spawned % _report_every) == 0 then
    _logger.log("Spawned " + _total_clients_spawned.string() + " clients.")
    end

  fun _try_shutdown() =>
    if not _should_continue() then
      _wait_for_shutdown()
    end

  be _wait_for_shutdown() =>
    if (_clients_spawned.size() == 0) and (not _finished) then
      _finished = true
      match _listener
      | let l: TCPListener =>
        _logger.log("All clients spawned and accounted for.")
        _logger.log(_failed.string()
          + " connections were unable to be established.")
        l.dispose()
        _listener = None
      | None =>
        _Unreachable()
      end
    else
      if _clients_spawned.size() != _last_size then
        _last_size = _clients_spawned.size()
        _logger.log("Waiting for final clients to finish."
          + _clients_spawned.size().string() + " remaining.")
      end
      _wait_for_shutdown()
    end

actor Logger
  let _out: OutStream

  new create(out: OutStream) =>
    _out = out

  be log(msg: String) =>
    let date = try
      let now = Time.now()
      PosixDate(now._1, now._2).format("%F %T")?
    else
      "n/a"
    end

    _out.print(date + ": " + msg)

primitive _Unreachable
  """
  To be used in places that the compiler can't prove is unreachable but we are
  certain is unreachable and if we reach it, we'd be silently hiding a bug.
  """
  fun apply(loc: SourceLoc = __loc) =>
    @fprintf(
      @pony_os_stderr(),
      ("The unreachable was reached in %s at line %s\n" +
       "Please open an issue at https://github.com/ponylang/ponyc/issues")
       .cstring(),
      loc.file().cstring(),
      loc.line().string().cstring())
    @exit(1)
